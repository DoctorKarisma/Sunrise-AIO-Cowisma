#include <Windows.h>

#include <cstdint>

#include "../../../runtime/storage/internal.h"
#include "../../events/activity_event_selection.h"
#include "../activity_membership_query.h"
#include "internal.h"

namespace sunrise::state::activity::membership {
namespace {

/**
 * Applies one client identity operation to current State.
 * @param state Activity State held under the root write lock.
 * @param record Target joined session record.
 * @param prepared Identity plan, consumed here.
 * @return True when the candidate matches and commits, or is unchanged.
 */
[[nodiscard]] bool commit_identity(ActivityState& state,
                                   SessionRecord& record,
                                   const PendingMutation& prepared) noexcept {
    if (!prepared.hasSnapshot
        || !transactions::equal(prepared.snapshot.identity, prepared.identityGuard)
        || !transactions::valid_identity(prepared.snapshot.identity, record.memberKey)) {
        return false;
    }
    // Current State decides the outcome. Comparing it against the prepared plan and refusing on a
    // difference would drop the identity whenever State moved between prepare and commit, so no
    // membership would ever publish.
    const bool changed =
        !record.membership.hasIdentity
        || !transactions::equal(record.membership.identity, prepared.snapshot.identity);
    if (changed
        && (state.stateRevision == activity::kMaximumRevision
            || record.membership.revision == kMaximumMembershipRevision)) {
        return false;
    }
    if (!changed) {
        return true;
    }
    const std::uint32_t revision =
        record.membership.hasIdentity ? record.membership.revision + 1U : kInitialRevision;

    MembershipState updated = record.membership;
    if (!updated.hasTransitionToken) {
        updated.transitionToken = kInitialTransitionToken;
        updated.hasTransitionToken = true;
    }
    updated.identity = prepared.snapshot.identity;
    updated.revision = revision;
    updated.acknowledgedRevision = kAbsentRevision;
    updated.hasIdentity = true;
    record.membership = updated;
    transactions::publish_change(state, record);
    return true;
}

/**
 * Checks one refresh plan against current State and keeps the bubble it named.
 * @param record Target joined session record.
 * @param prepared Refresh plan, consumed here.
 * @return True when the request guard and snapshot still match.
 */
[[nodiscard]] bool commit_refresh(SessionRecord& record, const PendingMutation& prepared) noexcept {
    if (prepared.refreshRequestGuard
        != transactions::refresh_guard(prepared.requestedRevision, prepared.bubbleIndex)) {
        return false;
    }
    if (record.membership.hasIdentity) {
        if (!prepared.hasSnapshot) {
            return false;
        }
        const Snapshot expected = transactions::make_snapshot(
            record.membership, record.membership.identity, record.membership.revision);
        if (!transactions::equal(prepared.snapshot, expected)) {
            return false;
        }
    } else if (prepared.hasSnapshot) {
        return false;
    }
    // The bubble is the client saying which slice set it holds. It is not a published field, so
    // no revision moves.
    record.membership.bubble = prepared.bubbleIndex;
    record.membership.bubbleRevision = prepared.requestedRevision;
    return true;
}

/** Applies one prepared revision advance to the exact current membership snapshot. */
[[nodiscard]] bool commit_republish(ActivityState& state,
                                    SessionRecord& record,
                                    const PendingMutation& prepared) noexcept {
    if (!prepared.hasSnapshot || !record.membership.hasIdentity
        || record.membership.revision == kMaximumMembershipRevision
        || prepared.snapshot.revision != record.membership.revision + 1U
        || !transactions::equal(prepared.snapshot.identity, record.membership.identity)) {
        return false;
    }
    ++record.membership.revision;
    record.membership.acknowledgedRevision = kAbsentRevision;
    transactions::publish_change(state, record);
    return true;
}

/**
 * Applies one membership acknowledgement to current State.
 * @param state Activity State held under the root write lock.
 * @param record Target joined session record.
 * @param prepared Acknowledgement plan, consumed here.
 * @return True when the mark commits or the revision is a no-op.
 */
[[nodiscard]] bool commit_acknowledgement(ActivityState& state,
                                          SessionRecord& record,
                                          const PendingMutation& prepared) noexcept {
    const bool changed = record.membership.hasIdentity
                         && prepared.acknowledgement == record.membership.revision
                         && prepared.acknowledgement != record.membership.acknowledgedRevision;
    if (changed && state.stateRevision == activity::kMaximumRevision) {
        return false;
    }
    if (changed) {
        record.membership.acknowledgedRevision = prepared.acknowledgement;
        transactions::publish_change(state, record);
    }
    return true;
}

} // namespace

/** Commits one identity, client-state, refresh, or acknowledgement operation. */
bool commit(PendingMutation& mutation, CommittedClientState* clientState) noexcept {
    if (clientState != nullptr) {
        *clientState = {};
    }
    const PendingMutation prepared = mutation;
    mutation = {};
    if (!prepared.prepared || prepared.kind == MutationKind::none
        || prepared.sessionId == kAbsentSessionId
        || prepared.expectedStateRevision == kInvalidRevision
        || prepared.expectedRecordRevision == kInvalidRevision
        || prepared.targetSlot >= kSessionCapacity) {
        return false;
    }

    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    auto& root = runtime::storage::g_state;
    ActivityState& state = root.activity;
    SessionRecord& record = state.sessions[prepared.targetSlot];
    bool committed = state.stateRevision == prepared.expectedStateRevision && record.occupied
                     && record.joined && record.joinedRevision != kInvalidRevision
                     && record.sessionId == prepared.sessionId
                     && record.recordRevision == prepared.expectedRecordRevision
                     && root.account.primarySoid == prepared.expectedPrimarySoid;
    if (committed && prepared.kind == MutationKind::identity) {
        committed = commit_identity(state, record, prepared);
    } else if (committed && prepared.kind == MutationKind::authoritative) {
        CommittedClientState after{};
        committed = transactions::commit_authoritative(state, record, prepared, after);
        if (committed && clientState != nullptr) {
            *clientState = after;
        }
    } else if (committed && prepared.kind == MutationKind::refresh) {
        committed = commit_refresh(record, prepared);
    } else if (committed && prepared.kind == MutationKind::republish) {
        committed = commit_republish(state, record, prepared);
    } else if (committed && prepared.kind == MutationKind::acknowledgement) {
        committed = commit_acknowledgement(state, record, prepared);
    } else {
        committed = false;
    }
    const bool refreshEventSelection =
        committed && prepared.kind == MutationKind::identity;

    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);

    // A membership identity commit is the fresh activity-join boundary. Promote the Events
    // page's pending roster selection here, after releasing the root State lock, so the roster
    // built for the new Tower/Farm instance sees the newly saved withheld-key set.
    if (refreshEventSelection) {
        events::reload();
    }

    return committed;
}

} // namespace sunrise::state::activity::membership
