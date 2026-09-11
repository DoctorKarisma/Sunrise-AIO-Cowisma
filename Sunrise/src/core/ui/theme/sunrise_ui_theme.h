#pragma once

#include <imgui.h>

namespace sunrise::core::ui::theme {

/** Applies the Sunrise colors and a fresh DPI-scaled copy of every authored size. */
void apply() noexcept;

/** @return The current color in the slow animated RGB border cycle. */
[[nodiscard]] ImVec4 animated_border_color() noexcept;

} // namespace sunrise::core::ui::theme
