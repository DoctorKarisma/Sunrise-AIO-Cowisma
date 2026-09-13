#include "festival_pickups.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include "../../../../middleware/bap/activity_message/incident.h"
#include "../../../../middleware/bap/activity_message/loot_pickup.h"
#include "../../../../state/activity/events/activity_event_selection.h"
#include "../../../../state/activity/runtime.h"
#include "../../../../state/build_data/runtime.h"
#include "../../../../state/runtime/runtime.h"
#include "../../internal.h"

namespace sunrise::server::bap::encrypted::festival_pickups {
namespace {

namespace incident = middleware::bap::activity_message::incident;
namespace loot = middleware::bap::activity_message::loot_pickup;

constexpr std::uint32_t kCandyDefinitionHash = 4084398230U;

struct Reward {
    std::uint32_t source{};
    std::int32_t amount{};
};

// Authored item_loot identifiers at +0x638 of AF3F/46/48/4A/4C (Courtyard)
// and AF2E/30/32/34 (Bazaar), build 86657. Translated props retain these identities.
// Courtyard blue = 50 Candy, Bazaar blue = 60 Candy, and both purple pickups = 250 Candy.
constexpr std::array<Reward, 9> kRewards{{
    {0xE86DC713U, 50},
    {0xE86DC710U, 50},
    {0xE86DC711U, 50},
    {0xE86DC716U, 50},
    {0xB6D6DC59U, 250},
    {0xE86DC717U, 60},
    {0xE86DC714U, 60},
    {0xE86DC715U, 60},
    {0xB6D6DC5AU, 250},
}};

struct Claim {
    state::activity::SessionBinding activity{};
    std::uint64_t characterSoid{};
    std::uint32_t sourceHash{};
    std::int32_t bubble{-1};
    bool occupied{};
};

// BAP's route lock serializes activity ingress. Keep one claim per authored pickup for the life of
// an activity generation so duplicate client incidents cannot award Candy twice.
std::array<Claim, 256> g_claims{};

[[nodiscard]] bool festival_visible(std::int32_t bubble) noexcept {
    std::uint32_t key{};
    switch (bubble) {
    case 6:
        key = 0x7C6DE64FU;
        break;
    case 1:
        key = 0xFC6B8707U;
        break;
    case 7:
        key = 0xEE34BBABU;
        break;
    case 0:
        key = 0x6D3740C6U;
        break;
    default:
        return false;
    }

    return !state::activity::events::withheld(key);
}

[[nodiscard]] Claim* reserve_claim(const state::activity::SessionBinding& activity,
                                   const loot::Pickup& pickup) noexcept {
    Claim* available = nullptr;

    for (auto& claim : g_claims) {
        if (claim.occupied && claim.activity.sessionId == activity.sessionId
            && claim.activity.createdRevision == activity.createdRevision
            && claim.characterSoid == pickup.characterSoid && claim.sourceHash == pickup.sourceHash
            && claim.bubble == pickup.bubble) {
            return nullptr;
        }

        if (!claim.occupied || !state::activity::binding_matches(claim.activity)) {
            if (available == nullptr) {
                available = &claim;
            }
        }
    }

    return available;
}

} // namespace

void receive(const ActivityClientBinding& binding,
             const middleware::bap::activity_message::Request& request) noexcept {
    constexpr std::string_view tower = "city_tower_social_d2";

    const auto& destination = binding.session.destination;
    if (destination.packageNameLength != tower.size()
        || !std::equal(tower.begin(), tower.end(), destination.packageName.begin())) {
        return;
    }

    incident::Incident framed{};
    loot::Pickup pickup{};

    if (incident::validate(request.payload, framed) != incident::Verdict::accepted
        || framed.primaryTarget != loot::kIncidentTarget || framed.extraTargetCount != 0
        || framed.hasCompressedSelector || framed.hasOptionalBlock
        || !loot::parse(std::span(framed.payload).first(framed.payloadLength), pickup)
        || !festival_visible(pickup.bubble)) {
        return;
    }

    const auto account = state::account_snapshot();
    if (pickup.accountSoid != account.primarySoid
        || pickup.characterSoid != state::account::selected_character_soid(account)) {
        return;
    }

    const auto reward =
        std::find_if(kRewards.begin(), kRewards.end(), [&pickup](const Reward& row) {
            return row.source == pickup.sourceHash;
        });
    if (reward == kRewards.end()) {
        return;
    }

    Claim* const claim = reserve_claim(binding.session, pickup);
    if (claim == nullptr) {
        return;
    }

    state::build_data::items::Definition candy{};
    if (!state::build_data::find_item_definition_hash(kCandyDefinitionHash, candy)
        || !bap::arm_world_profile_item_acquisition(candy.definitionIndex, reward->amount)) {
        return;
    }

    *claim = {
        binding.session,
        pickup.characterSoid,
        pickup.sourceHash,
        pickup.bubble,
        true,
    };
}

} // namespace sunrise::server::bap::encrypted::festival_pickups
