#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../../state/account/inventory/inventory_state.h"
#include "../instance/instance_encoder.h"

namespace sunrise::middleware::datagen::family4::loadout {

/** Character inventory items without an equipment slot retain an out-of-range native sentinel. */
inline constexpr std::uint8_t kUnavailableEquipmentSlot = 0xFFU;

/** Equipped plus unequipped authored items one character may publish. */
inline constexpr std::size_t kItemCapacity = state::account::inventory::kEquipmentSlotCount
                                             + state::account::inventory::kCharacterItemCapacity;

/** One installed-build-resolved item ready for character and instance encoding. */
struct ResolvedItem {
    std::uint16_t inventoryRow{};
    std::uint8_t equipmentSlot{};
    /** Only equipped items publish their instance SOID into the native equipment array. */
    bool equipped{};
    std::int32_t quantity{};
    /** Authored runtime generation copied into the native inventory row. */
    std::int32_t mutationSerial{};
    /** Accumulated native item-state bits copied into the native inventory row. */
    std::uint32_t flags{};
    instance::ResolvedInstance instance{};
    bool seen{};
};

/** One item instance together with the native equipment slot that owns it. */
struct SlottedInstance {
    std::uint8_t equipmentSlot{};
    instance::ResolvedInstance instance{};
};

/** Every item instance one character owns, whether or not that character is selected. */
struct ResolvedInstances {
    std::array<SlottedInstance, kItemCapacity> items{};
    std::size_t itemCount{};
};

/** Complete selected-character loadout committed only after every mapping resolves. */
struct ResolvedLoadout {
    std::array<ResolvedItem, kItemCapacity> items{};
    std::size_t itemCount{};
    std::uint32_t nextInventorySerial{};
};

} // namespace sunrise::middleware::datagen::family4::loadout
