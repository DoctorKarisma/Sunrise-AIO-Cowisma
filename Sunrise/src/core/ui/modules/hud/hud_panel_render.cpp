/**
 * The HUD page. One switch per overlay, in the order the overlays stack on screen.
 */

#include <cstddef>
#include <imgui.h>

#include "../../components/section/ui_section_component.h"
#include "../../components/toggle/ui_toggle_component.h"
#include "../../hud/overlay.h"
#include "../../theme/sunrise_ui_theme.h"
#include "internal.h"

namespace sunrise::core::ui::modules::hud::internal {

/** Draws the HUD page inside the active Core UI frame. */
void draw() noexcept {
    ImGui::TextWrapped("Overlays draw in the top-left corner while the game runs, with or "
                       "without this menu open.");
    ImGui::Spacing();

    for (std::size_t index = 0; index < static_cast<std::size_t>(ui::hud::Overlay::count);
         ++index) {
        const auto overlay = static_cast<ui::hud::Overlay>(index);
        bool on = ui::hud::enabled(overlay);

        if (components::toggle::control(ui::hud::display_name(overlay), on)) {
            ui::hud::set_enabled(overlay, on);
        }
    }

    ImGui::Spacing();
    components::section::header("Current status lines",
                                "Each line of the current status overlay, on its own.");
    ImGui::Spacing();

    for (std::size_t index = 0; index < static_cast<std::size_t>(ui::hud::StatusLine::count);
         ++index) {
        const auto line = static_cast<ui::hud::StatusLine>(index);
        bool on = ui::hud::enabled(line);

        if (components::toggle::control(ui::hud::display_name(line), on)) {
            ui::hud::set_enabled(line, on);
        }
    }

    ImGui::Spacing();
    components::section::header("Themes",
                                "Choose the base Sunrise appearance and optional animation.");
    ImGui::Spacing();

    const theme::Style selectedTheme = ui::hud::selected_theme();

    /*
     * Keep the theme selector and animation checkbox together on one compact row:
     *
     * Theme  [ RGB v ]    [ ] Animated
     */
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Theme");
    ImGui::SameLine();

    constexpr float kComboWidth = 180.0F;
    ImGui::SetNextItemWidth(kComboWidth);

    if (ImGui::BeginCombo("##hud_theme", theme::display_name(selectedTheme))) {
        constexpr theme::Style kThemes[]{
            theme::Style::sunriseOriginal,
            theme::Style::rgb,
        };

        for (const theme::Style candidate : kThemes) {
            const bool selected = candidate == selectedTheme;

            if (ImGui::Selectable(theme::display_name(candidate), selected)) {
                ui::hud::set_selected_theme(candidate);
            }

            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();

    bool animated = ui::hud::animated_theme();

    if (ImGui::Checkbox("Animated", &animated)) {
        ui::hud::set_animated_theme(animated);
    }
}

} // namespace sunrise::core::ui::modules::hud::internal
