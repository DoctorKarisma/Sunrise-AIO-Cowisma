#include "../../core/logging/log.h"
#include "../../state/build_data/runtime.h"
#include "../../state/investment/store_internal.h"
#include "internal.h"

namespace sunrise::server::bap {
namespace {

/** Grants and retires one earned reward in the same database transaction. */
bool commit_world_reward(const WorldRewardRequest& request) noexcept {
    state::investment::store::Transaction transaction;
    if (!transaction.ready()) {
        return false;
    }
    bool committed = false;
    if (request.kind == WorldRewardKind::item) {
        state::PendingItemAcquisition acquisition;
        committed =
            state::prepare_item_acquisition_for_item(request.itemDefinitionIndex, acquisition)
            && state::commit_item_acquisition(acquisition);
    } else {
        state::PendingProfileItemAcquisition acquisition;
        committed = state::prepare_profile_item_acquisition_for_item(
                        request.itemDefinitionIndex, request.quantity, acquisition)
                    && state::commit_profile_item_acquisition(acquisition);
    }
    return committed && complete_world_reward(request.id) && transaction.commit();
}

/** Saves the reward under its stable definition hash and earning character. */
bool enqueue_world_reward(std::uint16_t definitionIndex,
                          std::int32_t quantity,
                          WorldRewardKind kind) noexcept {
    state::build_data::items::Definition definition;
    if (!state::build_data::find_item_definition_index(definitionIndex, definition)
        || !state::investment::store::enqueue_reward(
            definition.definitionHash, quantity, static_cast<std::uint8_t>(kind))) {
        return false;
    }
    if (!has_active_family4_peer()) {
        settle_world_reward();
    }
    return true;
}

} // namespace

/** Saves one item reward before its pickup presentation is queued. */
bool arm_world_item_acquisition(std::uint16_t itemDefinitionIndex) noexcept {
    return enqueue_world_reward(itemDefinitionIndex, 1, WorldRewardKind::item);
}

/** Saves a profile material reward before its pickup presentation is queued. */
bool arm_world_profile_item_acquisition(std::uint16_t itemDefinitionIndex,
                                        std::int32_t quantity) noexcept {
    return quantity > 0
           && enqueue_world_reward(itemDefinitionIndex, quantity, WorldRewardKind::profileItem);
}

/** Restores the oldest reward belonging to the selected character. */
bool current_world_reward(WorldRewardRequest& request) noexcept {
    request = {};
    state::investment::store::PendingReward saved;
    state::build_data::items::Definition definition;
    if (!state::investment::store::next_reward(saved)
        || !state::build_data::find_item_definition_hash(saved.definitionHash, definition)) {
        return false;
    }
    request.id = saved.id;
    request.quantity = saved.quantity;
    request.itemDefinitionIndex = definition.definitionIndex;
    request.kind = static_cast<WorldRewardKind>(saved.kind);
    return true;
}

/** Acknowledges exactly the reward whose inventory grant is being committed. */
bool complete_world_reward(std::uint64_t id) noexcept {
    return state::investment::store::complete_reward(id);
}

/** A failed grant stays saved for a later attempt. */
void settle_world_reward() noexcept {
    WorldRewardRequest request;
    if (current_world_reward(request) && commit_world_reward(request)) {
        arm_account_resync_everywhere();
    }
}

/** Shutdown may grant ready rewards; rewards for other characters stay in the database. */
void drain_world_rewards() noexcept {
    WorldRewardRequest request;
    while (current_world_reward(request)) {
        if (!commit_world_reward(request)) {
            break;
        }
    }
}

} // namespace sunrise::server::bap
