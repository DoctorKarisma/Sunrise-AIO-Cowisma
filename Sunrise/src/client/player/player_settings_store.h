#pragma once

#include <cstdint>

namespace sunrise::client::player {

/** Lowest FOV accepted by the target client. */
inline constexpr std::int32_t kMinimumFieldOfView = 55;
/** Highest FOV accepted by the target client. */
inline constexpr std::int32_t kMaximumFieldOfView = 150;
/** Neutral starting value used until the user picks one. */
inline constexpr std::int32_t kDefaultFieldOfView = 85;

/** Runtime player configuration. This module owns it; Core settings do not carry it. */
struct Settings {
    bool infiniteAmmoEnabled{false};
    /** Holds every activity inactivity timeout at its longest. */
    bool antiAfkEnabled{false};

    /** When true, the camera hook applies fieldOfView every frame. */
    bool fieldOfViewOverrideEnabled{false};
    /** Horizontal field of view in degrees. */
    std::int32_t fieldOfView{kDefaultFieldOfView};
};

/**
 * Resolves the configuration file and loads it when one exists.
 * @param module Loaded DLL used to resolve the owned artifact directory.
 */
void initialize(void* module) noexcept;

/** Drops the runtime configuration and the resolved file path. */
void shutdown() noexcept;

/** @return One lock-consistent copy of the current configuration. */
[[nodiscard]] Settings get() noexcept;

/**
 * Publishes one configuration and writes it straight to disk.
 * @param settings Configuration to store.
 * @return True when the value was published. A failed write is logged, not returned.
 */
bool publish(const Settings& settings) noexcept;

} // namespace sunrise::client::player
