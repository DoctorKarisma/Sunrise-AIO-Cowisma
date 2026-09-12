/** The player module's interface. Every control saves at once, so a change survives a restart. */

#include "player_panel.h"

#include <imgui.h>

#include "../../../core/ui/components/toggle/ui_toggle_component.h"
#include "../../hooks/godmode/godmode.h"
#include "../../hooks/no_turnback/no_turnback.h"
#include "../../hooks/player_scale/player_scale.h"
#include "../../player/player_settings_store.h"

namespace sunrise::client::ui::player {

/** Draws the player module inside the active Core UI frame. */
void draw() noexcept {
    namespace toggle = core::ui::components::toggle;

    client::player::Settings settings = client::player::get();

    static bool playerScaleApplyFailed = false;

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
    ImGui::TextWrapped("Adjust the field of view in real time.");
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

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextUnformatted("Player Size");
    ImGui::Separator();
    ImGui::TextWrapped("Change the local player's uniform object scale. "
                       "Some first-person or attached character pieces may not scale correctly.");
    ImGui::Spacing();

    /*
     * IMPORTANT:
     *
     * Opening the menu never calls player_scale::apply().
     *
     * We only touch the player when the user explicitly changes the toggle,
     * slider, or presses Reset.
     */
    if (toggle::control("Enabled##player_scale", settings.playerScaleEnabled)) {

        const float wanted = settings.playerScaleEnabled ? settings.playerScale
                                                         : client::player::kDefaultPlayerScale;

        playerScaleApplyFailed = !hooks::player_scale::apply(wanted);

        (void)client::player::publish(settings);
    }

    ImGui::BeginDisabled(!settings.playerScaleEnabled);

    float playerScale = settings.playerScale;

    if (ImGui::SliderFloat("##player_scale",
                           &playerScale,
                           client::player::kMinimumPlayerScale,
                           client::player::kMaximumPlayerScale,
                           "%.2fx")) {

        settings.playerScale = playerScale;

        playerScaleApplyFailed = !hooks::player_scale::apply(settings.playerScale);

        (void)client::player::publish(settings);
    }

    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Reset##player_scale")) {

        settings.playerScale = client::player::kDefaultPlayerScale;

        playerScaleApplyFailed = !hooks::player_scale::apply(client::player::kDefaultPlayerScale);

        (void)client::player::publish(settings);
    }

    if (playerScaleApplyFailed) {

        ImGui::TextDisabled("Player scale could not be applied.");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextUnformatted("No Turnback");
    ImGui::Separator();
    ImGui::TextWrapped("Disable turnback-zone enforcement.");
    ImGui::Spacing();

    if (toggle::control("Enabled##no_turnback", settings.noTurnbackEnabled)) {

        if (hooks::no_turnback::set_enabled(settings.noTurnbackEnabled)) {

            (void)client::player::publish(settings);

        } else {

            settings.noTurnbackEnabled = !settings.noTurnbackEnabled;
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextUnformatted("Godmode");
    ImGui::Separator();
    ImGui::TextWrapped("Prevent the player from taking damage.");
    ImGui::Spacing();

    if (toggle::control("Enabled##godmode", settings.godmodeEnabled)) {

        if (hooks::godmode::set_enabled(settings.godmodeEnabled)) {

            (void)client::player::publish(settings);

        } else {

            settings.godmodeEnabled = !settings.godmodeEnabled;
        }
    }
}

} // namespace sunrise::client::ui::player
