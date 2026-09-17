#include "progression_bank_keys.h"

#include <array>
#include <type_traits>

#include "../../../../state/build_data/runtime.h"
#include "../../../../state/unlocks/unlocks_runtime.h"

namespace sunrise::middleware::datagen::family4::progression {
namespace {

/** Both replicated banks hold 127 rows. */
constexpr std::size_t kBankCapacity = 127;
/** All bits set is the only value the record enumerator treats as an empty slot. */
constexpr std::uint16_t kEmptyDefinitionIndex = 0xFFFF;

// One lane type serves the authored bank and the native row, so no conversion can lose a lane.
static_assert(std::is_same_v<layout::Values, state::unlocks::ProgressionLanes>);

} // namespace

/** Keys one object's progression bank and fills each keyed row from the authored lanes. */
bool key_bank(state::build_data::progressions::Scope scope,
              std::span<layout::Entry> bank) noexcept {
    for (layout::Entry& entry : bank) {
        entry = layout::Entry{};
        entry.definitionIndex = kEmptyDefinitionIndex;
    }
    std::array<std::uint16_t, kBankCapacity> slots{};
    std::size_t count = 0;
    if (!state::build_data::find_progression_slots(scope, slots, count) || count > bank.size()) {
        return false;
    }
    state::unlocks::Table table;
    if (!state::unlocks::snapshot(table)) {
        return false;
    }
    const auto& lanes = scope == state::build_data::progressions::Scope::account
                            ? table.accountProgressions
                            : table.characterProgressions;
    for (std::size_t slot = 0; slot < count; ++slot) {
        // The definition catalog is dense, so every key it hands out addresses the authored bank.
        bank[slot].definitionIndex = slots[slot];
        bank[slot].values = lanes[slots[slot]];
    }
    return true;
}

} // namespace sunrise::middleware::datagen::family4::progression
