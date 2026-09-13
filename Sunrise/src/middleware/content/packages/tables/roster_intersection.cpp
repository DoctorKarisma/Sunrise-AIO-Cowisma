#include "roster_intersection.h"

namespace sunrise::middleware::content::packages::tables {
namespace {

constexpr std::uint32_t kSliceSetIndexBound = 512;

[[nodiscard]] bool slice_set_bit(std::uint32_t sliceSetIndex, std::uint32_t& bit) noexcept {
    bit = 0;
    if (sliceSetIndex >= kSliceSetIndexBound || sliceSetIndex % kSliceSetIndexFactor != 0) {
        return false;
    }
    bit = sliceSetIndex / kSliceSetIndexFactor;
    return true;
}

} // namespace

bool is_event_roster_key(std::uint32_t registryKey) noexcept {
    for (const std::uint32_t key : kEventRosterKeys) {
        if (key == registryKey) {
            return true;
        }
    }
    return false;
}

bool is_selectable_event_key(std::uint32_t registryKey) noexcept {
    for (const std::uint32_t key : kSelectableEventKeys) {
        if (key == registryKey) {
            return true;
        }
    }
    return false;
}

bool carries_roster_slot(std::span<const std::byte> object) noexcept {
    Array slots{};
    if (!object_slots(object, slots)) {
        return false;
    }

    std::uint32_t key = 0;
    if (object_key(object, key)) {
        for (const std::uint32_t forced : kForcedRosterKeys) {
            if (key == forced) {
                return true;
            }
        }
        if (is_event_roster_key(key)) {
            return true;
        }
    }

    for (std::uint64_t index = 0; index < slots.count; ++index) {
        Slot slot{};
        if (!object_slot_at(object, slots, index, slot)) {
            return false;
        }
        for (const std::uint16_t wanted : kRosterSlotTypes) {
            if (slot.type == wanted) {
                return true;
            }
        }
    }
    return false;
}

bool observe_slice_set(RosterIntersection& state, std::uint32_t sliceSetIndex) noexcept {
    std::uint32_t bit = 0;
    if (!slice_set_bit(sliceSetIndex, bit)) {
        state.overflowed = true;
        return false;
    }
    ++state.stateCount;
    ++state.bubbleStateCounts[bit];
    state.currentBubble = bit;
    state.stateOpen = true;
    state.observedSets |= std::uint64_t{1} << bit;
    return true;
}

bool observe_roster_key(RosterIntersection& state,
                        std::uint32_t sliceSetIndex,
                        std::uint32_t objectKey) noexcept {
    std::uint32_t bit = 0;
    if (!slice_set_bit(sliceSetIndex, bit)) {
        state.overflowed = true;
        return false;
    }
    if (!state.stateOpen || bit != state.currentBubble || state.stateCount == 0) {
        state.overflowed = true;
        return false;
    }

    const std::uint64_t mask = std::uint64_t{1} << bit;

    for (std::size_t index = 0; index < state.keyCount; ++index) {
        if (state.keys[index] == objectKey) {
            state.masks[index] |= mask;
            if (state.lastKeyState[index] != state.stateCount) {
                state.lastKeyState[index] = state.stateCount;
                ++state.keyStateCounts[index];
                ++state.keyBubbleStateCounts[index][bit];
            }
            return true;
        }
    }

    if (state.keyCount == kRosterKeyCapacity) {
        state.overflowed = true;
        return false;
    }

    state.keys[state.keyCount] = objectKey;
    state.masks[state.keyCount] = mask;
    state.lastKeyState[state.keyCount] = state.stateCount;
    state.keyStateCounts[state.keyCount] = 1;
    state.keyBubbleStateCounts[state.keyCount][bit] = 1;
    ++state.keyCount;
    return true;
}

void observe_unresolved_slice_set(RosterIntersection& state) noexcept {
    state.unresolvedSet = true;
}

bool safe_roster_keys(const RosterIntersection& state,
                      std::span<std::uint32_t> output,
                      std::size_t& count) noexcept {
    count = 0;
    if (state.overflowed) {
        return false;
    }
    if (state.unresolvedSet) {
        return true;
    }

    for (std::size_t index = 0; index < state.keyCount; ++index) {
        if (state.stateCount == 0 || state.keyStateCounts[index] != state.stateCount) {
            continue;
        }
        if (count == output.size()) {
            count = 0;
            return false;
        }
        output[count++] = state.keys[index];
    }
    return true;
}

bool partial_roster_keys(const RosterIntersection& state,
                         std::span<std::uint32_t> keys,
                         std::span<std::uint64_t> masks,
                         std::size_t& count) noexcept {
    count = 0;
    if (state.overflowed || keys.size() != masks.size()) {
        return false;
    }
    if (state.unresolvedSet) {
        return true;
    }

    for (std::size_t index = 0; index < state.keyCount; ++index) {
        std::uint64_t mask = 0;
        for (std::size_t bubble = 0; bubble < kSliceSetCapacity; ++bubble) {
            if (state.bubbleStateCounts[bubble] != 0
                && state.keyBubbleStateCounts[index][bubble] == state.bubbleStateCounts[bubble]) {
                mask |= std::uint64_t{1} << bubble;
            }
        }
        if (mask == 0 || mask == state.observedSets) {
            continue;
        }
        if (count == keys.size()) {
            count = 0;
            return false;
        }
        keys[count] = state.keys[index];
        masks[count] = mask;
        ++count;
    }
    return true;
}

} // namespace sunrise::middleware::content::packages::tables
