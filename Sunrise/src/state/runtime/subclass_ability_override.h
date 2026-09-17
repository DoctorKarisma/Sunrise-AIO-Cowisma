#pragma once

#include <cstdint>

#include "state/build_data/abilities/definition.h"

namespace sunrise::state::runtime::subclass_ability_override {

enum class Slot : std::uint8_t {
    movement = 0,
    grenade = 1,
    super = 2,
    melee = 3,
    classAbility = 4,
    secondary1 = 5,
    secondary2 = 6,
};

struct Choice {
    bool enabled{};
    std::uint16_t sourceSocketEntryListIndex{};
    build_data::abilities::Selection sourceSelection{};
    std::uint8_t sourceEntry{};
    /** 0..2 when this override selects a native subclass path; 0xFF for a standalone ability. */
    std::uint8_t sourcePath{0xFF};
    /** For Secondary Perk slots, the individual native tree-node hash. Source fields retain its bucket routing. */
    std::uint32_t secondaryHash{};
    /** Secondary nodes may transplant their native source bucket by semantic kind across subclasses/classes. */
    bool secondaryNativeTransplant{};
};

[[nodiscard]] bool set(std::uint64_t characterSoid, Slot slot, const Choice& choice) noexcept;
[[nodiscard]] bool get(std::uint64_t characterSoid, Slot slot, Choice& choice) noexcept;
void clear(std::uint64_t characterSoid, Slot slot) noexcept;
void clear_character(std::uint64_t characterSoid) noexcept;

} // namespace sunrise::state::runtime::subclass_ability_override
