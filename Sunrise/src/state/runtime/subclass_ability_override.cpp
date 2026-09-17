#include "subclass_ability_override.h"

#include <array>
#include <cstddef>
#include <Windows.h>

namespace sunrise::state::runtime::subclass_ability_override {
namespace {

constexpr std::size_t kCharacterCapacity = 8;
constexpr std::size_t kSlotCapacity = 7;

struct Row {
    std::uint64_t characterSoid{};
    std::array<Choice, kSlotCapacity> choices{};
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::array<Row, kCharacterCapacity> g_rows{};

[[nodiscard]] std::size_t slot_index(Slot slot) noexcept {
    return static_cast<std::size_t>(slot);
}

[[nodiscard]] Row* find_row(std::uint64_t characterSoid) noexcept {
    for (Row& row : g_rows) {
        if (row.characterSoid == characterSoid) {
            return &row;
        }
    }
    return nullptr;
}

[[nodiscard]] Row* ensure_row(std::uint64_t characterSoid) noexcept {
    if (Row* row = find_row(characterSoid); row != nullptr) {
        return row;
    }
    for (Row& row : g_rows) {
        if (row.characterSoid == 0) {
            row = {};
            row.characterSoid = characterSoid;
            return &row;
        }
    }
    return nullptr;
}

} // namespace

bool set(std::uint64_t characterSoid, Slot slot, const Choice& choice) noexcept {
    const std::size_t index = slot_index(slot);
    if (characterSoid == 0 || index >= kSlotCapacity || !choice.enabled) {
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    Row* row = ensure_row(characterSoid);
    if (row != nullptr) {
        row->choices[index] = choice;
    }
    ReleaseSRWLockExclusive(&g_lock);
    return row != nullptr;
}

bool get(std::uint64_t characterSoid, Slot slot, Choice& choice) noexcept {
    choice = {};
    const std::size_t index = slot_index(slot);
    if (characterSoid == 0 || index >= kSlotCapacity) {
        return false;
    }
    AcquireSRWLockShared(&g_lock);
    const Row* row = find_row(characterSoid);
    if (row != nullptr) {
        choice = row->choices[index];
    }
    ReleaseSRWLockShared(&g_lock);
    return choice.enabled;
}

void clear(std::uint64_t characterSoid, Slot slot) noexcept {
    const std::size_t index = slot_index(slot);
    if (characterSoid == 0 || index >= kSlotCapacity) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    if (Row* row = find_row(characterSoid); row != nullptr) {
        row->choices[index] = {};
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void clear_character(std::uint64_t characterSoid) noexcept {
    if (characterSoid == 0) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    if (Row* row = find_row(characterSoid); row != nullptr) {
        *row = {};
    }
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace sunrise::state::runtime::subclass_ability_override
