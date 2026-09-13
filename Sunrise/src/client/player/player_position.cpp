/**
 * The local player's published world position.
 * The game threads write it and the interface reads it, so a seqlock guards the vector.
 */

#include "player_position.h"

#include <atomic>
#include <cstdint>

namespace sunrise::client::player::position {
namespace {

namespace teleport = hooks::teleport;

/** Odd while a write is in progress, so a reader that sees one retries. */
std::atomic_uint32_t g_sequence{0};

/** Written between two sequence bumps, and read between two equal even reads. */
teleport::Vector g_position{};

/** True after at least one valid player position has been published. */
std::atomic_bool g_present{false};

/**
 * The player's physics component, found on the sync tick.
 * It is kept here rather than taken from the teleport hook, because that hook only caches one
 * while the teleport feature is switched on.
 */
std::atomic<void*> g_component{nullptr};

/**
 * Reads one component's body position and publishes it.
 *
 * @param component Component already proved to be the player's.
 * @return True when the body was read.
 *
 * A failed read intentionally leaves the last valid position published. Physics bodies can
 * disappear temporarily while the game changes character state, respawns, or transitions.
 */
[[nodiscard]] bool publish_from(void* component) noexcept {
    teleport::Vector position{};

    if (!teleport::read_position(component, position)) {
        return false;
    }

    g_sequence.fetch_add(1, std::memory_order_acq_rel);
    g_position = position;
    g_sequence.fetch_add(1, std::memory_order_release);

    g_present.store(true, std::memory_order_release);
    return true;
}

} // namespace

/** Publishes the position of the component the physics sync is running for. */
void observe(void* component) noexcept {
    if (component == nullptr) {
        return;
    }

    void* const known = g_component.load(std::memory_order_relaxed);

    if (known == component) {
        (void)publish_from(component);
        return;
    }

    /*
     * The ownership test is paid only until the player's component is known.
     *
     * poll() drops a stale cached component. Once that happens, the next physics sync belonging
     * to the local player is allowed to establish the replacement here.
     */
    if (known != nullptr || !teleport::owns_local_player(component)) {
        return;
    }

    g_component.store(component, std::memory_order_relaxed);
    (void)publish_from(component);
}

/**
 * Refreshes the position for a player at rest and releases a component that no longer belongs
 * to the local player.
 *
 * Losing ownership of the cached component does NOT erase the last valid position. Character
 * components can be replaced transiently, and clearing g_present on a single failed ownership
 * check makes the HUD flash between valid coordinates and "waiting for a position".
 *
 * Dropping only g_component allows observe() to acquire the replacement on its next physics sync
 * while readers continue to see the last known-good position.
 */
void poll() noexcept {
    void* component = g_component.load(std::memory_order_relaxed);

    if (component == nullptr) {
        /*
         * The teleport hook may also know the current player component.
         * Use it as a recovery source when our own observer has not acquired one yet.
         */
        component = teleport::local_player_component();
    }

    if (component == nullptr) {
        return;
    }

    if (!teleport::owns_local_player(component)) {
        /*
         * The component is stale or is no longer the player's.
         *
         * Release it so observe() can discover the replacement, but preserve g_present and the
         * last valid coordinate. A transient ownership miss is not proof that the player has no
         * position.
         */
        g_component.store(nullptr, std::memory_order_relaxed);
        return;
    }

    g_component.store(component, std::memory_order_relaxed);
    (void)publish_from(component);
}

/** Drops the cached component and the published position. */
void reset() noexcept {
    g_component.store(nullptr, std::memory_order_relaxed);
    g_present.store(false, std::memory_order_release);
}

/** @return The last published position. */
Snapshot snapshot() noexcept {
    Snapshot value{};

    if (!g_present.load(std::memory_order_acquire)) {
        return value;
    }

    for (;;) {
        const std::uint32_t before = g_sequence.load(std::memory_order_acquire);

        if ((before & 1U) != 0U) {
            continue;
        }

        value.position = g_position;

        if (g_sequence.load(std::memory_order_acquire) == before) {
            break;
        }
    }

    value.present = true;
    return value;
}

} // namespace sunrise::client::player::position
