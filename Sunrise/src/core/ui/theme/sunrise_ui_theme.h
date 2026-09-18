#pragma once

#include <cstdint>
#include <imgui.h>

namespace sunrise::core::ui::theme {

/** Base visual theme. Animation is an independent presentation option. */
enum class Style : std::uint8_t {
    sunriseOriginal,
    rgb,
};

/** Applies the selected colors and a fresh DPI-scaled copy of every authored size. */
void apply() noexcept;

/**
 * Updates colors that change continuously while a frame is running.
 * Call once after ImGui::NewFrame().
 */
void update() noexcept;

/** @return The currently selected base theme. */
[[nodiscard]] Style selected() noexcept;

/**
 * Selects the base theme and immediately applies it when an ImGui context exists.
 * @param style New base theme.
 */
void set_selected(Style style) noexcept;

/** @return Display name for a base theme. */
[[nodiscard]] const char* display_name(Style style) noexcept;

/** @return True when the optional animation layer is enabled. */
[[nodiscard]] bool animated() noexcept;

/**
 * Enables or disables the optional animation layer.
 * ImAnim will consume this setting once that layer is integrated.
 */
void set_animated(bool enabled) noexcept;

/** @return The current color in the slow animated RGB cycle. */
[[nodiscard]] ImVec4 animated_border_color() noexcept;

} // namespace sunrise::core::ui::theme
