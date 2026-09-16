#pragma once

#include <cstdint>
#include <d3d11.h>
#include <imgui.h>

namespace sunrise::client::hooks::graphics::textures {

/** One uploaded image: the texture we own and the view the interface draws with. */
struct Uploaded {
    ID3D11Texture2D* texture{};
    ID3D11ShaderResourceView* view{};
};

/**
 * Decodes the bundled logo sheet and publishes its view to the Core interface.
 * @param device Device that creates and owns the texture.
 * @param output Receives the created objects. Left alone when any step fails.
 * @return True when the sheet is uploaded and published.
 */
[[nodiscard]] bool upload_logo_sheet(ID3D11Device* device, Uploaded& output) noexcept;

/** @param uploaded Objects released and cleared, after the published slot is emptied. */
void release_logo_sheet(Uploaded& uploaded) noexcept;

/**
 * Returns the cached ImGui texture for one item definition hash.
 * The Gear Editor uses the bundled item-icons resource to populate this cache.
 *
 * @param definitionHash Authored Destiny item definition hash.
 * @return ImGui texture handle, or null when the icon is unavailable.
 */
[[nodiscard]] ImTextureID item_icon(std::uint32_t definitionHash) noexcept;

/** Releases every cached Gear Editor item-icon texture. */
void release_item_icons() noexcept;

} // namespace sunrise::client::hooks::graphics::textures
