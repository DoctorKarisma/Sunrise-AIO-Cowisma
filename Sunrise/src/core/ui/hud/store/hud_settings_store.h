#pragma once

#include <cstddef>
#include <span>

namespace sunrise::core::ui::hud::store {

/** One overlay's file key and its switch state. */
struct Switch {
    const char* key{};
    bool on{};
};

/**
 * Resolves the HUD settings file. It reads nothing; load does that.
 * @param module Loaded DLL used to resolve the owned artifact directory.
 */
void initialize(void* module) noexcept;

/** Drops the resolved file path. */
void shutdown() noexcept;

/**
 * Applies saved HUD state over the caller's defaults.
 *
 * Missing or malformed values leave the supplied defaults untouched, so older hud.json files
 * remain valid after theme settings are introduced.
 *
 * @param switches Overlay/status keys to load.
 * @param theme Receives the saved theme name when present.
 * @param themeCapacity Capacity of the theme output buffer.
 * @param animated Receives the saved animation switch when present.
 */
void load(std::span<Switch> switches,
          char* theme,
          std::size_t themeCapacity,
          bool& animated) noexcept;

/**
 * Writes the complete HUD settings file.
 *
 * @param switches Every overlay/status key and its state, in table order.
 * @param theme Stable theme storage name.
 * @param animated Animation-layer switch.
 * @return True when every byte reached the file.
 */
bool save(std::span<const Switch> switches, const char* theme, bool animated) noexcept;

} // namespace sunrise::core::ui::hud::store
