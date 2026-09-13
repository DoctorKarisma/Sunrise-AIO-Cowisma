#include "investment_overrides.h"

#include <Windows.h>

#include <atomic>
#include <cstddef>

#include "../runtime/storage/internal.h"
#include "investment.h"

namespace sunrise::state::investment {
namespace {

/**
 * Sets or replaces one row of a bounded override list. The caller holds the state lock.
 * @param rows List storage.
 * @param count Rows in use, advanced on an append.
 * @param slot Slot to set.
 * @param value Value stored beside the slot.
 * @return False when the slot was absent and the list is full.
 */
template <typename Row, std::size_t Capacity, typename Value>
[[nodiscard]] bool set_row(std::array<Row, Capacity>& rows,
                           std::size_t& count,
                           std::uint16_t slot,
                           Value value) noexcept {
    for (std::size_t index = 0; index < count && index < rows.size(); ++index) {
        if (rows[index].slot == slot) {
            rows[index].value = value;
            return true;
        }
    }
    if (count >= rows.size()) {
        return false;
    }
    rows[count].slot = slot;
    rows[count].value = value;
    ++count;
    return true;
}

/**
 * Removes one row of a bounded override list. Order is not kept; the client reads the list as a
 * set. The caller holds the state lock.
 */
template <typename Row, std::size_t Capacity>
void clear_row(std::array<Row, Capacity>& rows, std::size_t& count, std::uint16_t slot) noexcept {
    for (std::size_t index = 0; index < count && index < rows.size(); ++index) {
        if (rows[index].slot == slot) {
            --count;
            rows[index] = rows[count];
            rows[count] = {};
            return;
        }
    }
}

std::atomic_bool g_refetchRequested{false};

} // namespace

/** Asks the client to fetch its family-5 object again. */
void request_client_refetch() noexcept {
    g_refetchRequested.store(true, std::memory_order_release);
}

/** @return True once per request. */
bool consume_client_refetch() noexcept {
    return g_refetchRequested.exchange(false, std::memory_order_acq_rel);
}

/** Sets or replaces one flag override. */
bool set_flag_override(std::uint16_t slot, std::uint8_t value) noexcept {
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    Family5State& family = runtime::storage::g_state.investment.family5;
    const bool stored = set_row(family.flags, family.flagCount, slot, value);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return stored;
}

/** Removes one flag override. */
void clear_flag_override(std::uint16_t slot) noexcept {
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    Family5State& family = runtime::storage::g_state.investment.family5;
    clear_row(family.flags, family.flagCount, slot);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
}

/** Sets or replaces one value override. */
bool set_value_override(std::uint16_t slot, std::int32_t value) noexcept {
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    Family5State& family = runtime::storage::g_state.investment.family5;
    const bool stored = set_row(family.values, family.valueCount, slot, value);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return stored;
}

/** Removes one value override. */
void clear_value_override(std::uint16_t slot) noexcept {
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    Family5State& family = runtime::storage::g_state.investment.family5;
    clear_row(family.values, family.valueCount, slot);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
}

} // namespace sunrise::state::investment
