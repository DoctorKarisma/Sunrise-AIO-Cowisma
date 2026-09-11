/** The player module's interface. Every control saves at once, so a change survives a restart. */

#include "player_panel.h"

#include <imgui.h>

#include "../../../core/ui/components/toggle/ui_toggle_component.h"
#include "../../player/player_settings_store.h"

namespace sunrise::client::ui::player {

/** Draws the player module inside the active Core UI frame. */
void draw() noexcept {
    namespace toggle = core::ui::components::toggle;

    client::player::Settings settings = client::player::get();

    ImGui::TextUnformatted("Infinite Ammo");
    ImGui::Separator();
    ImGui::TextWrapped("Keep every weapon's reserves full.");
    ImGui::Spacing();

    bool changed = toggle::control("Enabled##infinite_ammo", settings.infiniteAmmoEnabled);

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextUnformatted("Anti AFK");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Disable AFK timeouts from activities kicking to orbit and the title screen.");
    ImGui::Spacing();

    changed = toggle::control("Enabled##anti_afk", settings.antiAfkEnabled) || changed;

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextUnformatted("Field of View");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Override the game's camera FOV.");
    ImGui::Spacing();

    changed =
        toggle::control("Enabled##field_of_view", settings.fieldOfViewOverrideEnabled) || changed;

    ImGui::BeginDisabled(!settings.fieldOfViewOverrideEnabled);

    int fieldOfView = static_cast<int>(settings.fieldOfView);

    if (ImGui::SliderInt("##field_of_view",
                         &fieldOfView,
                         static_cast<int>(client::player::kMinimumFieldOfView),
                         static_cast<int>(client::player::kMaximumFieldOfView),
                         "%d")) {
        settings.fieldOfView = static_cast<std::int32_t>(fieldOfView);
        changed = true;
    }

    ImGui::EndDisabled();

    if (changed) {
        (void)client::player::publish(settings);
    }
}

} // namespace sunrise::client::ui::player
