#pragma once

#include <cstdint>

#include "../theme/sunrise_ui_theme.h"

namespace sunrise::core::ui::hud {

/** Every HUD overlay, in the order the menu lists them and the corner stacks them. */
enum class Overlay : std::uint8_t {
    /** The Sunrise name, version and animated logo. */
    logoCard,
    /** The local player's current world coordinates. */
    coordinates,
    /** Where the player is: activity, bubble, slice set and closest spawn. */
    currentStatus,
    /** The instances of the session the player is in. */
    session,
    /** Recent owned client messages at the Activity Host boundary. */
    sensorEvents,
    /** What the mission script VM is doing, per attached activity. */
    missionScript,
    count,
};

/** Every line of the current-status overlay, each with its own switch. */
enum class StatusLine : std::uint8_t {
    activity,
    bubble,
    sliceSet,
    closestSpawn,
    count,
};

/**
 * Resolves the HUD settings file and applies its saved state.
 * @param module Loaded DLL used to resolve the owned artifact directory.
 */
void initialize(void* module) noexcept;

/** Drops the HUD settings file path. Runtime values remain intact. */
void shutdown() noexcept;

/** @param overlay Overlay to name. @return Its menu label. */
[[nodiscard]] const char* display_name(Overlay overlay) noexcept;

/** @param overlay Overlay to read. @return True while it draws. */
[[nodiscard]] bool enabled(Overlay overlay) noexcept;

/** @param overlay Overlay to switch. @param on New switch state. */
void set_enabled(Overlay overlay, bool on) noexcept;

/** @param line Status line to name. @return Its menu label. */
[[nodiscard]] const char* display_name(StatusLine line) noexcept;

/** @param line Status line to read. @return True while the status line draws. */
[[nodiscard]] bool enabled(StatusLine line) noexcept;

/** @param line Status line to switch. @param on New switch state. */
void set_enabled(StatusLine line, bool on) noexcept;

/** @return Currently selected base UI theme. */
[[nodiscard]] theme::Style selected_theme() noexcept;

/** Selects and saves a base UI theme. */
void set_selected_theme(theme::Style style) noexcept;

/** @return True while the optional animation layer is enabled. */
[[nodiscard]] bool animated_theme() noexcept;

/** Enables/disables and saves the optional animation layer. */
void set_animated_theme(bool enabled) noexcept;

/**
 * Draws every enabled overlay, stacked down the top-left corner. It runs whether the menu is
 * open or not.
 * @param interfaceEnabled Core UI enabled state.
 * @return True when draw data was built for at least one overlay.
 */
[[nodiscard]] bool draw(bool interfaceEnabled) noexcept;

} // namespace sunrise::core::ui::hud
