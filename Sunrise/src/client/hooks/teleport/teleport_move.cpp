/**
 * The teleport itself. The camera hook publishes a forward vector and reads the bound key once a
 * frame. The physics hook applies the move before the sync it runs ahead of. Physics owns the
 * position, so writing the object placement would move the camera alone.
 */

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "../../../core/logging/log.h"
#include "../../../core/ui/runtime/ui_visibility_runtime.h"
#include "../../../state/account/account_state.h"
#include "../../../state/runtime/runtime.h"
#include "../../input/window_focus.h"
#include "../../movement/movement_settings_store.h"
#include "../../player/player_settings_store.h"
#include "../polled_input/runtime.h"
#include "internal.h"
#include "runtime.h"

namespace sunrise::client::hooks::teleport {
namespace {

/**
 * Frames a press stays pending. Orbit and loading screens tick the camera but never the player's
 * physics, so a request with no limit is used up later and reads as a queued teleport.
 */
constexpr std::uint32_t kRequestLifetimeFrames = 3;
/** Frames an ordinary physics tick gets to collect a request before the forced path takes it. */
constexpr std::uint32_t kForceAfterFrames = 1;

/**
 * Frames the injected press is held. It has to survive at least one scan and one integration
 * step, or the move it exists to publish is never read.
 */
constexpr std::uint32_t kPressFrames = 2;
/** Authored action driven to wake the body. Forward is the gentlest one that moves it. */
constexpr std::uint16_t kForwardAction =
    static_cast<std::uint16_t>(state::account::settings::bindings::Action::moveForward);

/** Degrees-to-radians conversion constant for the live FOV override. */
constexpr float kPi = 3.14159265358979323846F;

std::atomic_bool g_requested{false};
std::atomic_bool g_forwardValid{false};
std::atomic_bool g_keyDown{false};
std::atomic_uint32_t g_requestAge{0};
/** Set while the feature is usable, so the per-tick path costs one atomic read when it is not. */
std::atomic_bool g_active{false};
SRWLOCK g_cameraPoseLock{SRWLOCK_INIT};
CameraPose g_cameraPose{};
bool g_cameraPoseValid{};

/**
 * The player's physics component, kept from the last tick that carried it. At rest the sync stops
 * being called for the player at all, so the pointer is the only way back to them.
 */
std::atomic<std::byte*> g_playerComponent{nullptr};
/** Frames left before the injected press is released. */
std::atomic_uint32_t g_pressFrames{0};

ControlledHandle g_controlledHandle{};
CameraSingleton g_cameraSingleton{};

/** Camera forward vector for the next physics tick. Every access holds g_cameraPoseLock. */
std::array<float, kVectorLanes> g_forward{};

/** Withdraws the pose when the camera block is not readable for this frame. */
void invalidate_camera_pose() noexcept {
    AcquireSRWLockExclusive(&g_cameraPoseLock);
    g_cameraPose = {};
    g_cameraPoseValid = false;
    ReleaseSRWLockExclusive(&g_cameraPoseLock);
}

/** @param forward Receives the published camera forward vector, copied under the pose lock. */
void copy_forward(Vector& forward) noexcept {
    AcquireSRWLockShared(&g_cameraPoseLock);
    forward = g_forward;
    ReleaseSRWLockShared(&g_cameraPoseLock);
}

/**
 * Reads one value out of game memory without faulting on a torn pointer.
 * @param address Source address.
 * @param value Receives the value.
 * @return True when Windows copied the whole value.
 */
template <typename T> [[nodiscard]] bool read_at(const std::byte* address, T& value) noexcept {
    if (address == nullptr) {
        return false;
    }

    SIZE_T read = 0;

    return ReadProcessMemory(GetCurrentProcess(), address, &value, sizeof value, &read) != FALSE
           && read == sizeof value;
}

/**
 * Writes one vector into game memory.
 * @param address Destination address.
 * @param value Three lanes to store.
 * @return True when Windows copied the whole vector.
 */
[[nodiscard]] bool write_vector(std::byte* address,
                                const std::array<float, kVectorLanes>& value) noexcept {

    if (address == nullptr) {
        return false;
    }

    SIZE_T written = 0;
    const SIZE_T size = sizeof(float) * kVectorLanes;

    return WriteProcessMemory(GetCurrentProcess(), address, value.data(), size, &written) != FALSE
           && written == size;
}

/**
 * Writes one floating-point value into game memory.
 * @param address Destination address.
 * @param value Value to store.
 * @return True when the complete value was written.
 */
[[nodiscard]] bool write_float(std::byte* address, float value) noexcept {
    if (address == nullptr) {
        return false;
    }

    SIZE_T written = 0;

    return WriteProcessMemory(GetCurrentProcess(), address, &value, sizeof value, &written) != FALSE
           && written == sizeof value;
}

/**
 * Finds the rigid body a physics component drives.
 * @param component Physics component.
 * @return The body, or null when the chain breaks.
 */
[[nodiscard]] std::byte* body_of(std::byte* component) noexcept {
    std::byte* array = nullptr;
    std::int32_t index = 0;

    if (!read_at(component + kPhysicsComponentBodyArray, array)
        || !read_at(component + kPhysicsComponentBodyIndex, index) || array == nullptr
        || index < 0) {
        return nullptr;
    }

    std::byte* body = nullptr;

    const std::size_t offset = kBodyEntryStride * static_cast<std::size_t>(index) + kBodyPointer;

    return read_at(array + offset, body) ? body : nullptr;
}

/**
 * Ages a pending request and drops it once nothing has taken it.
 */
void expire_request() noexcept {
    if (!g_requested.load(std::memory_order_acquire)) {
        return;
    }

    if (g_requestAge.fetch_add(1, std::memory_order_relaxed) + 1 >= kRequestLifetimeFrames) {
        g_requested.store(false, std::memory_order_release);
    }
}

/**
 * Runs the whole move for a component already proved to be the player's.
 */
[[nodiscard]] bool perform_move(std::byte* component) noexcept;

/** @param reason Key naming the step that stopped the move. */
void report_skip(const char* reason) noexcept;

/**
 * Starts the injected press that wakes the body.
 */
void begin_press() noexcept {
    const state::AccountState account = state::account_snapshot();

    const auto& binding = account.settings.keyBindings.values[kForwardAction];

    if (!binding.primary.has_value()) {
        return;
    }

    const std::uint32_t virtualKey = action_key(*binding.primary);

    if (virtualKey == 0) {
        report_skip("no_key");
        return;
    }

    hooks::polled_input::hold_key(virtualKey);

    g_pressFrames.store(kPressFrames, std::memory_order_release);
}

/** Releases the injected press once it has been scanned. */
void end_press() noexcept {
    if (g_pressFrames.load(std::memory_order_acquire) == 0) {
        return;
    }

    if (g_pressFrames.fetch_sub(1, std::memory_order_acq_rel) <= 1) {
        hooks::polled_input::release_key();
    }
}

/**
 * @param component Candidate physics component.
 * @return True when it drives the object the local player controls.
 */
[[nodiscard]] bool owns_player(std::byte* component) noexcept {
    if (component == nullptr || g_controlledHandle == nullptr) {
        return false;
    }

    std::uint32_t controlled = kInvalidHandle;

    g_controlledHandle(&controlled);

    if (controlled == kInvalidHandle) {
        return false;
    }

    std::uint16_t owner = 0;

    return read_at(component + kPhysicsComponentObjectHandle, owner)
           && (controlled & kHandleIndexMask)
                  == (static_cast<std::uint32_t>(owner) & kHandleIndexMask);
}

/** @param reason Key naming the step that stopped the move. */
void report_skip(const char* reason) noexcept {
    std::array<char, 96> line{};

    const int written = std::snprintf(
        line.data(), line.size(), "ev=teleport stage=move result=skip reason=%s", reason);

    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/**
 * Writes one vertical velocity, leaving run momentum on the other two lanes.
 */
void set_vertical_velocity(std::byte* body, float value) noexcept {

    std::array<float, kVectorLanes> velocity{};

    if (!read_at(body + kBodyVelocityX, velocity)) {
        return;
    }

    velocity[kVerticalLane] = value;

    (void)write_vector(body + kBodyVelocityX, velocity);
}

/**
 * Adds one world delta to a stored position.
 */
[[nodiscard]] bool offset_vector(std::byte* address,
                                 const std::array<float, kVectorLanes>& delta,
                                 std::array<float, kVectorLanes>& before,
                                 std::array<float, kVectorLanes>& after) noexcept {

    if (!read_at(address, before)) {
        return false;
    }

    for (std::size_t lane = 0; lane < kVectorLanes; ++lane) {
        after[lane] = before[lane] + delta[lane];
    }

    return write_vector(address, after);
}

/**
 * Adds the configured distance along the published forward vector.
 */
[[nodiscard]] bool move_body(std::byte* body, float distance) noexcept {

    Vector forward{};
    copy_forward(forward);

    std::array<float, kVectorLanes> delta{};

    for (std::size_t lane = 0; lane < kVectorLanes; ++lane) {
        delta[lane] = forward[lane] * distance;
    }

    std::array<float, kVectorLanes> position{};
    std::array<float, kVectorLanes> moved{};

    if (!offset_vector(body + kBodyPositionX, delta, position, moved)) {
        report_skip("body");
        return false;
    }

    std::array<char, 160> line{};

    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=teleport stage=move result=ok dist=%.1f "
                                      "from=%.1f,%.1f,%.1f to=%.1f,%.1f,%.1f",
                                      static_cast<double>(distance),
                                      static_cast<double>(position[0]),
                                      static_cast<double>(position[1]),
                                      static_cast<double>(position[2]),
                                      static_cast<double>(moved[0]),
                                      static_cast<double>(moved[1]),
                                      static_cast<double>(moved[2]));

    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }

    return true;
}

/**
 * Runs the whole move for a component already proved to be the player's.
 */
[[nodiscard]] bool perform_move(std::byte* component) noexcept {

    std::byte* const body = body_of(component);

    if (body == nullptr) {
        report_skip("no_body");
        return false;
    }

    set_vertical_velocity(body, 0.0F);

    if (!move_body(body, client::movement::get().distance)) {
        return false;
    }

    begin_press();

    return true;
}

} // namespace

/** Publishes the two functions the hooks call. */
void publish_targets(ControlledHandle controlled, CameraSingleton singleton) noexcept {

    g_controlledHandle = controlled;
    g_cameraSingleton = singleton;
}

/** Drops those functions and every latched request. */
void clear_targets() noexcept {
    g_controlledHandle = nullptr;
    g_cameraSingleton = nullptr;

    g_requested.store(false, std::memory_order_release);
    g_forwardValid.store(false, std::memory_order_relaxed);
    g_keyDown.store(false, std::memory_order_relaxed);
    g_requestAge.store(0, std::memory_order_relaxed);
    g_active.store(false, std::memory_order_relaxed);
    g_playerComponent.store(nullptr, std::memory_order_relaxed);

    invalidate_camera_pose();
}

/** Publishes the frame's complete camera pose and its forward vector. */
void capture_camera_pose(std::uint32_t playerIndex) noexcept {

    if (playerIndex == kInvalidHandle || g_cameraSingleton == nullptr) {

        g_playerComponent.store(nullptr, std::memory_order_relaxed);

        invalidate_camera_pose();
        return;
    }

    std::byte* const camera = g_cameraSingleton();

    if (camera == nullptr) {
        g_playerComponent.store(nullptr, std::memory_order_relaxed);

        invalidate_camera_pose();
        return;
    }

    const std::size_t playerOffset = kCameraBlockStride * playerIndex;

    const client::player::Settings playerSettings = client::player::get();

    if (playerSettings.fieldOfViewOverrideEnabled) {
        const float radians = static_cast<float>(playerSettings.fieldOfView) * (kPi / 180.0F);

        (void)write_float(camera + playerOffset + kCameraHorizontalFov, radians);
    }

    CameraPose pose{};

    if (!read_at(camera + playerOffset + kCameraPositionX, pose.position)
        || !read_at(camera + playerOffset + kCameraForwardX, pose.forward)
        || !read_at(camera + playerOffset + kCameraUpX, pose.up)
        || !read_at(camera + playerOffset + kCameraHorizontalFov, pose.horizontalFov)
        || !read_at(camera + playerOffset + kCameraAspect, pose.aspect)) {

        invalidate_camera_pose();
        return;
    }

    AcquireSRWLockExclusive(&g_cameraPoseLock);

    g_cameraPose = pose;
    g_cameraPoseValid = true;
    g_forward = pose.forward;

    ReleaseSRWLockExclusive(&g_cameraPoseLock);

    g_forwardValid.store(true, std::memory_order_release);
}

/** Latches one teleport request if the bound key went down this frame. */
void poll_request() noexcept {
    end_press();
    expire_request();

    const client::movement::Settings settings = client::movement::get();

    const bool usable = settings.enabled && settings.virtualKey != client::movement::kNoKey;

    g_active.store(usable, std::memory_order_relaxed);

    if (!usable) {
        g_keyDown.store(false, std::memory_order_relaxed);
        return;
    }

    if (core::ui::runtime::snapshot().visible) {
        g_keyDown.store(false, std::memory_order_relaxed);
        return;
    }

    const bool down = client::input::game_focused()
                      && (GetAsyncKeyState(static_cast<int>(settings.virtualKey)) & 0x8000) != 0;

    if (down && !g_keyDown.exchange(down, std::memory_order_relaxed)) {

        g_requestAge.store(0, std::memory_order_relaxed);

        g_requested.store(true, std::memory_order_release);

        return;
    }

    g_keyDown.store(down, std::memory_order_relaxed);
}

/**
 * Observes the player's physics component even when Teleport itself is disabled.
 */
void apply_pending(void* component) noexcept {
    if (component == nullptr || g_controlledHandle == nullptr) {
        return;
    }

    std::byte* const physics = static_cast<std::byte*>(component);

    std::byte* cached = g_playerComponent.load(std::memory_order_relaxed);

    if (cached == nullptr) {
        if (!owns_player(physics)) {
            return;
        }

        g_playerComponent.store(physics, std::memory_order_relaxed);

        cached = physics;
    }

    if (!g_active.load(std::memory_order_relaxed)) {
        return;
    }

    const bool requested = g_requested.load(std::memory_order_acquire);

    if (cached != physics) {
        if (!requested || !owns_player(physics)) {
            return;
        }

        g_playerComponent.store(physics, std::memory_order_relaxed);

        cached = physics;
    }

    if (!requested || !g_forwardValid.load(std::memory_order_acquire)) {
        return;
    }

    g_requested.store(false, std::memory_order_release);

    (void)perform_move(cached);
}

/** Runs the move for a request no physics tick collected. */
void force_pending() noexcept {
    if (!g_requested.load(std::memory_order_acquire)
        || !g_forwardValid.load(std::memory_order_acquire)
        || g_requestAge.load(std::memory_order_relaxed) < kForceAfterFrames) {
        return;
    }

    std::byte* const physics = g_playerComponent.load(std::memory_order_relaxed);

    if (physics == nullptr || g_controlledHandle == nullptr || !owns_player(physics)) {
        return;
    }

    g_requested.store(false, std::memory_order_release);

    if (!perform_move(physics)) {
        return;
    }

    invoke_sync(physics);

    core::log::write(
        core::log::Channel::client, core::log::Level::info, "ev=teleport stage=force result=ok");
}

/** Reports the physics component the local player was last seen driving. */
void* local_player_component() noexcept {
    return g_playerComponent.load(std::memory_order_relaxed);
}

/** Reports the complete datum handle of the object the local player controls. */
std::uint32_t local_player_handle() noexcept {
    if (g_controlledHandle == nullptr) {
        return kInvalidHandle;
    }

    std::uint32_t controlled = kInvalidHandle;

    g_controlledHandle(&controlled);

    return controlled;
}

/** @return True when the local player drives the candidate component. */
bool owns_local_player(void* component) noexcept {

    return component != nullptr && g_controlledHandle != nullptr
           && owns_player(static_cast<std::byte*>(component));
}

/** Reads the world position of the body a physics component drives. */
bool read_position(void* component, Vector& position) noexcept {

    if (component == nullptr) {
        return false;
    }

    std::byte* const body = body_of(static_cast<std::byte*>(component));

    return body != nullptr && read_at(body + kBodyPositionX, position);
}

/** Writes the world position of the body a physics component drives. */
bool write_position(void* component, const Vector& position) noexcept {

    if (component == nullptr) {
        return false;
    }

    std::byte* const body = body_of(static_cast<std::byte*>(component));

    return body != nullptr && write_vector(body + kBodyPositionX, position);
}

/** Reads the linear velocity of the body a physics component drives. */
bool read_velocity(void* component, Vector& velocity) noexcept {

    if (component == nullptr) {
        return false;
    }

    std::byte* const body = body_of(static_cast<std::byte*>(component));

    return body != nullptr && read_at(body + kBodyVelocityX, velocity);
}

/** Finds the rigid body a physics component drives. */
void* body(void* component) noexcept {
    return component != nullptr ? body_of(static_cast<std::byte*>(component)) : nullptr;
}

/** Writes the linear velocity of the body a physics component drives. */
bool write_velocity(void* component, const Vector& velocity) noexcept {

    if (component == nullptr) {
        return false;
    }

    std::byte* const body = body_of(static_cast<std::byte*>(component));

    return body != nullptr && write_vector(body + kBodyVelocityX, velocity);
}

/** Reports the camera forward vector published this frame. */
bool camera_forward(Vector& forward) noexcept {
    if (!g_forwardValid.load(std::memory_order_acquire)) {
        return false;
    }

    copy_forward(forward);
    return true;
}

/** Copies the last complete pose published by the camera-frame hook. */
bool camera_pose(CameraPose& pose) noexcept {
    AcquireSRWLockShared(&g_cameraPoseLock);

    const bool valid = g_cameraPoseValid;

    pose = valid ? g_cameraPose : CameraPose{};

    ReleaseSRWLockShared(&g_cameraPoseLock);

    return valid;
}

} // namespace sunrise::client::hooks::teleport
