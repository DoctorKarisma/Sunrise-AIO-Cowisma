#pragma once

#include <cstdint>

namespace sunrise::client::player {

/** Lowest FOV accepted by the target client. */
inline constexpr std::int32_t kMinimumFieldOfView = 55;
/** Highest FOV accepted by the target client. */
inline constexpr std::int32_t kMaximumFieldOfView = 150;
/** Neutral starting value used until the user picks one. */
inline constexpr std::int32_t kDefaultFieldOfView = 85;

/** Smallest player scale exposed by the interface. */
inline constexpr float kMinimumPlayerScale = 0.25F;
/** Largest player scale exposed by the interface. */
inline constexpr float kMaximumPlayerScale = 10.0F;
/** Normal Destiny player scale. */
inline constexpr float kDefaultPlayerScale = 1.0F;

/** Normal game-time rate. Values above this make the world advance faster. */
inline constexpr float kDefaultWorldSpeed = 1.0F;
/** Slowest world-time multiplier accepted by the interface and settings file. */
inline constexpr float kMinimumWorldSpeed = 0.1F;
/** Fastest world-time multiplier accepted by the interface and settings file. */
inline constexpr float kMaximumWorldSpeed = 10.0F;

/** Runtime player configuration. This module owns it; Core settings do not carry it. */
struct Settings {
    bool infiniteAmmoEnabled{false};

    /** Holds every activity inactivity timeout at its longest. */
    bool antiAfkEnabled{false};

    /** When true, the camera hook applies fieldOfView every frame. */
    bool fieldOfViewOverrideEnabled{false};

    /** Horizontal field of view in degrees. */
    std::int32_t fieldOfView{kDefaultFieldOfView};

    /** Experimental uniform-scale override for the local player object. */
    bool playerScaleEnabled{false};

    /** 1.0 is the game's normal player size. */
    float playerScale{kDefaultPlayerScale};

    /** 1.0 is normal game speed. */
    float worldSpeed{kDefaultWorldSpeed};

    /** Prevents the turnback/out-of-bounds check from reporting the player outside the area. */
    bool noTurnbackEnabled{false};

    /** Prevents the normal damage instruction from reducing player health. */
    bool godmodeEnabled{false};
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
