#include "investment_overrides.h"

#include <atomic>
#include <cstddef>

#include "investment.h"
#include "store.h"
#include "store_internal.h"

namespace sunrise::state::investment {
namespace {

/**
 * Sets or replaces one row of a bounded override list.
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
 * Removes one row of a bounded override list.
 * Order is not kept; the client reads the list as a set.
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
    store::g_mutex.lock();

    store::Transaction transaction;
    Family5State family{};

    if (!transaction.ready() || !store::read_family5(family)) {
        store::g_mutex.unlock();
        return false;
    }

    const bool stored = set_row(family.flags, family.flagCount, slot, value)
                        && store::write_family5(family) && transaction.commit();

    store::g_mutex.unlock();
    return stored;
}

/** Removes one flag override. */
void clear_flag_override(std::uint16_t slot) noexcept {
    store::g_mutex.lock();

    store::Transaction transaction;
    Family5State family{};

    if (!transaction.ready() || !store::read_family5(family)) {
        store::g_mutex.unlock();
        return;
    }

    clear_row(family.flags, family.flagCount, slot);

    (void)(store::write_family5(family) && transaction.commit());

    store::g_mutex.unlock();
}

/** Sets or replaces one value override. */
bool set_value_override(std::uint16_t slot, std::int32_t value) noexcept {
    store::g_mutex.lock();

    store::Transaction transaction;
    Family5State family{};

    if (!transaction.ready() || !store::read_family5(family)) {
        store::g_mutex.unlock();
        return false;
    }

    const bool stored = set_row(family.values, family.valueCount, slot, value)
                        && store::write_family5(family) && transaction.commit();

    store::g_mutex.unlock();
    return stored;
}

/** Removes one value override. */
void clear_value_override(std::uint16_t slot) noexcept {
    store::g_mutex.lock();

    store::Transaction transaction;
    Family5State family{};

    if (!transaction.ready() || !store::read_family5(family)) {
        store::g_mutex.unlock();
        return;
    }

    clear_row(family.values, family.valueCount, slot);

    (void)(store::write_family5(family) && transaction.commit());

    store::g_mutex.unlock();
}

} // namespace sunrise::state::investment
