#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "scenario_reader.h"

namespace sunrise::middleware::content::packages::tables {

inline constexpr std::size_t kSliceSetCapacity = 64;
inline constexpr std::size_t kRosterKeyCapacity = 32;

static_assert(kSliceSetCapacity * kSliceSetIndexFactor == 512);
static_assert(kSliceSetCapacity == 64);

struct RosterIntersection {
    std::array<std::uint32_t, kRosterKeyCapacity> keys{};
    std::array<std::uint64_t, kRosterKeyCapacity> masks{};
    std::array<std::size_t, kRosterKeyCapacity> keyStateCounts{};
    std::array<std::array<std::size_t, kSliceSetCapacity>, kRosterKeyCapacity>
        keyBubbleStateCounts{};
    std::array<std::size_t, kRosterKeyCapacity> lastKeyState{};
    std::array<std::size_t, kSliceSetCapacity> bubbleStateCounts{};
    std::size_t keyCount{};
    std::size_t stateCount{};
    std::uint32_t currentBubble{};
    std::uint64_t observedSets{};
    bool stateOpen{};
    bool overflowed{};
    bool unresolvedSet{};
};

void observe_unresolved_slice_set(RosterIntersection& state) noexcept;

inline constexpr std::array<std::uint32_t, 1> kForcedRosterKeys = {0x432A36E6U};

/**
 * All 18 special Tower/Farm roster keys that need the special admission path.
 * Four entries are baseline support keys and are intentionally NOT menu-selectable.
 */
inline constexpr std::array<std::uint32_t, 18> kEventRosterKeys = {
    0x00ACD208U,
    0x27060E6CU,
    0x4F4ED92FU,
    0x50CC9C7DU,
    0x6CEFCC01U,
    0x7C6DE64FU,
    0xD5B68262U,
    0x6D3740C6U,
    0x2F2B8D00U,
    0x08E64D48U,
    0xFC6B8707U,
    0x099B0342U,
    0x9052672CU,
    0xDA989AA3U,
    0xEE34BBABU,
    0x6E087824U,
    0xC140FF19U,
    0x4488AD94U,
};

/** The 14 real seasonal keys controlled by the Events menu. */
inline constexpr std::array<std::uint32_t, 14> kSelectableEventKeys = {
    0x7C6DE64FU,
    0xFC6B8707U,
    0xEE34BBABU,
    0x6D3740C6U,
    0x00ACD208U,
    0x2F2B8D00U,
    0x6E087824U,
    0xDA989AA3U,
    0xC140FF19U,
    0x4488AD94U,
    0x27060E6CU,
    0x6CEFCC01U,
    0xD5B68262U,
    0x9052672CU,
};

inline constexpr std::array<std::uint16_t, 9> kRosterSlotTypes = {
    8, 13, 16, 17, 21, 35, 37, 41, 67};

[[nodiscard]] bool carries_roster_slot(std::span<const std::byte> object) noexcept;
[[nodiscard]] bool is_event_roster_key(std::uint32_t registryKey) noexcept;
[[nodiscard]] bool is_selectable_event_key(std::uint32_t registryKey) noexcept;
[[nodiscard]] bool observe_slice_set(RosterIntersection& state,
                                     std::uint32_t sliceSetIndex) noexcept;
[[nodiscard]] bool observe_roster_key(RosterIntersection& state,
                                      std::uint32_t sliceSetIndex,
                                      std::uint32_t objectKey) noexcept;
[[nodiscard]] bool safe_roster_keys(const RosterIntersection& state,
                                    std::span<std::uint32_t> output,
                                    std::size_t& count) noexcept;
[[nodiscard]] bool partial_roster_keys(const RosterIntersection& state,
                                       std::span<std::uint32_t> keys,
                                       std::span<std::uint64_t> masks,
                                       std::size_t& count) noexcept;

} // namespace sunrise::middleware::content::packages::tables
