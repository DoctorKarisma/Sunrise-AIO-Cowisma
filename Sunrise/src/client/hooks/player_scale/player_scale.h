#pragma once

namespace sunrise::client::hooks::player_scale {

/**
 * Changes only the controlled player's encoded uniform-scale lane.
 *
 * This scales the main player object. Some first-person/presentation
 * attachments may not inherit the scale correctly.
 *
 * @param scale Requested uniform scale.
 * @return True when the live player datum was found and updated.
 */
[[nodiscard]] bool apply(float scale) noexcept;

} // namespace sunrise::client::hooks::player_scale
