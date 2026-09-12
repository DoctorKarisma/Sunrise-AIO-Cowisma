#include "player_scale.h"

#include <Windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../teleport/runtime.h"

namespace sunrise::client::hooks::player_scale {
namespace {

/** Destiny's invalid datum sentinel. */
constexpr std::uint32_t kInvalidDatum = 0xFFFFFFFFU;

/**
 * Live object datum layout.
 *
 * These values are from the same object-datum table used by the working
 * Entity Spawner/runtime code.
 */
constexpr std::uintptr_t kObjectDatumDescriptorRva = 0x1F93420;
constexpr std::size_t kObjectDatumBaseOffset = 0x08;
constexpr std::size_t kObjectDatumStrideOffset = 0x10;
constexpr std::size_t kObjectDatumBytes = 0xE0;
constexpr std::size_t kObjectHandleOffset = 0x0C;

/**
 * ObjectTransform's processed translation/scale block:
 *
 * +0xB0 encoded X
 * +0xB4 encoded Y
 * +0xB8 encoded Z
 * +0xBC encoded uniform scale
 *
 * Player Size modifies ONLY +0xBC.
 *
 * We deliberately do not call Destiny's complete ObjectTransform routine for
 * the controlled player because doing so can affect position and cause
 * teleport/freeze behavior.
 */
constexpr std::size_t kObjectEncodedScaleOffset = 0xBC;

/** Destiny's normal object scale. */
constexpr float kNormalScale = 1.0F;

/**
 * Defensive limits.
 *
 * The UI also has its own limits, but apply() rejects invalid values in case
 * it is called from somewhere else.
 */
constexpr float kMinimumSafeScale = 0.05F;
constexpr float kMaximumSafeScale = 10.0F;

HMODULE g_gameModule{};

/**
 * State belonging to the currently controlled player datum.
 *
 * The encoded transform uses an XOR key. We recover that key from the encoded
 * scale lane and the scale value we know that lane currently represents.
 */
std::uint32_t g_cachedHandle{kInvalidDatum};
float g_lastScale{kNormalScale};
bool g_haveScaleState{};

/**
 * Reads one value from the current process without directly dereferencing an
 * unstable game pointer.
 */
template <typename Value> [[nodiscard]] bool safe_read(const void* source, Value& value) noexcept {

    if (source == nullptr) {
        value = {};
        return false;
    }

    SIZE_T read = 0;

    return ReadProcessMemory(GetCurrentProcess(), source, &value, sizeof value, &read) != FALSE
           && read == sizeof value;
}

/**
 * Writes one value into the current process.
 */
template <typename Value>
[[nodiscard]] bool safe_write(void* destination, const Value& value) noexcept {

    if (destination == nullptr) {
        return false;
    }

    SIZE_T written = 0;

    return WriteProcessMemory(GetCurrentProcess(), destination, &value, sizeof value, &written)
               != FALSE
           && written == sizeof value;
}

/** Returns the raw IEEE-754 representation of one float. */
[[nodiscard]] std::uint32_t float_bits(float value) noexcept {

    std::uint32_t bits = 0;

    static_assert(sizeof bits == sizeof value);

    std::memcpy(&bits, &value, sizeof bits);

    return bits;
}

/** Clears all state belonging to the previously controlled object. */
void clear_scale_state() noexcept {

    g_cachedHandle = kInvalidDatum;
    g_lastScale = kNormalScale;
    g_haveScaleState = false;
}

/**
 * Begins tracking a newly controlled player object.
 *
 * A fresh controlled object is expected to begin at Destiny's normal 1.0
 * object scale before Player Size modifies it.
 */
void begin_object(std::uint32_t handle) noexcept {

    g_cachedHandle = handle;
    g_lastScale = kNormalScale;
    g_haveScaleState = true;
}

/**
 * Resolves one generation-valid datum handle to its live object row.
 */
[[nodiscard]] std::byte* resolve_object(std::uint32_t handle) noexcept {

    if (handle == kInvalidDatum) {
        return nullptr;
    }

    if (g_gameModule == nullptr) {

        g_gameModule = GetModuleHandleW(nullptr);

        if (g_gameModule == nullptr) {
            return nullptr;
        }
    }

    std::byte* const descriptor =
        reinterpret_cast<std::byte*>(g_gameModule) + kObjectDatumDescriptorRva;

    std::byte* base = nullptr;
    std::uint32_t stride = 0;

    if (!safe_read(descriptor + kObjectDatumBaseOffset, base)
        || !safe_read(descriptor + kObjectDatumStrideOffset, stride) || base == nullptr
        || stride != kObjectDatumBytes) {

        return nullptr;
    }

    std::byte* const object = base + static_cast<std::size_t>(handle & 0x1FFFU) * stride;

    std::uint32_t liveHandle = kInvalidDatum;

    if (!safe_read(object + kObjectHandleOffset, liveHandle) || liveHandle != handle) {

        return nullptr;
    }

    return object;
}

} // namespace

bool apply(float scale) noexcept {

    /*
     * Reject invalid requests before touching game memory.
     */
    if (!std::isfinite(scale) || scale < kMinimumSafeScale || scale > kMaximumSafeScale) {

        return false;
    }

    /*
     * Teleport already owns the known-working controlled-player resolver.
     */
    const std::uint32_t handle = teleport::local_player_handle();

    if (handle == kInvalidDatum) {

        clear_scale_state();
        return false;
    }

    std::byte* const object = resolve_object(handle);

    if (object == nullptr) {

        clear_scale_state();
        return false;
    }

    /*
     * Respawns/activity transitions can replace the full datum generation.
     * Always compare the complete handle.
     */
    if (!g_haveScaleState || g_cachedHandle != handle) {

        begin_object(handle);
    }

    std::uint32_t encodedCurrent = 0;

    if (!safe_read(object + kObjectEncodedScaleOffset, encodedCurrent)) {

        return false;
    }

    /*
     * Destiny stores this scale lane as:
     *
     *     encoded = floatBits(scale) XOR key
     *
     * We know what value we previously wrote, allowing the XOR key to be
     * recovered from the current encoded value.
     */
    const std::uint32_t key = encodedCurrent ^ float_bits(g_lastScale);

    const std::uint32_t encodedWanted = key ^ float_bits(scale);

    /*
     * This is intentionally the ONLY game transform field Player Size writes.
     *
     * No X.
     * No Y.
     * No Z.
     * No rotation.
     * No child/attachment objects.
     * No presentation structures.
     * No ObjectTransform() call.
     */
    if (!safe_write(object + kObjectEncodedScaleOffset, encodedWanted)) {

        return false;
    }

    g_lastScale = scale;

    return true;
}

} // namespace sunrise::client::hooks::player_scale
