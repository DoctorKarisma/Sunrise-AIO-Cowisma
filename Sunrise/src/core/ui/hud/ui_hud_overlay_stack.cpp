/**
 * The HUD overlay stack. Overlays are fixed windows in the top-left corner: each one draws its
 * own content, and the stack places it under the one before it.
 */

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <imgui.h>

#include "../../../client/player/player_position.h"
#include "../scaling/dpi/ui_dpi_scaling.h"
#include "../theme/sunrise_ui_theme.h"
#include "overlay.h"
#include "overlays/ui_hud_logo_overlay.h"
#include "overlays/ui_hud_mission_script_overlay.h"
#include "overlays/ui_hud_sensor_events_overlay.h"
#include "overlays/ui_hud_session_overlay.h"
#include "overlays/ui_hud_status_overlay.h"
#include "store/hud_settings_store.h"

namespace sunrise::core::ui::hud {
namespace {

/** Draws the local player's current world coordinates as one compact HUD line. */
void draw_coordinates() noexcept {
    ImGui::TextDisabled("XYZ");
    ImGui::SameLine();

    const client::player::position::Snapshot snapshot = client::player::position::snapshot();

    if (!snapshot.present) {
        ImGui::TextUnformatted("--  --  --");
        return;
    }

    char coordinates[96]{};
    (void)std::snprintf(coordinates,
                        sizeof(coordinates),
                        "%.2f  %.2f  %.2f",
                        static_cast<double>(snapshot.position[0]),
                        static_cast<double>(snapshot.position[1]),
                        static_cast<double>(snapshot.position[2]));

    ImGui::TextUnformatted(coordinates);
}

/** One overlay's identity, frame entry and starting switch state. */
struct Entry {
    const char* displayName;
    const char* storageKey;
    const char* windowId;
    void (*draw)() noexcept;
    bool startsOn;
};

constexpr float kViewportMargin = 24.0F;
constexpr float kOverlayGap = 8.0F;

constexpr char kThemeSunriseOriginal[] = "sunrise_original";
constexpr char kThemeRgb[] = "rgb";

constexpr std::size_t kThemeStorageCapacity = 32;

constexpr ImGuiWindowFlags kOverlayFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav
    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
    | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove;

constexpr std::size_t kOverlayCount = static_cast<std::size_t>(Overlay::count);
constexpr std::size_t kStatusLineCount = static_cast<std::size_t>(StatusLine::count);
constexpr std::size_t kSwitchCount = kOverlayCount + kStatusLineCount;

constexpr std::array<Entry, kOverlayCount> kOverlays{
    Entry{"Sunrise Card", "sunrise_card", "##sunrise_hud_card", &overlays::logo::draw, true},
    Entry{"Coordinates", "coordinates", "##sunrise_hud_coordinates", &draw_coordinates, false},
    Entry{
        "Current Status", "current_status", "##sunrise_hud_status", &overlays::status::draw, false},
    Entry{"Session", "session", "##sunrise_hud_session", &overlays::session::draw, false},
    Entry{"Client Activity Messages",
          "sensor_events",
          "##sunrise_hud_sensor_events",
          &overlays::sensor_events::draw,
          false},
    Entry{"Mission Script",
          "mission_script",
          "##sunrise_hud_mission_script",
          &overlays::mission_script::draw,
          false},
};

struct LineEntry {
    const char* displayName;
    const char* storageKey;
    bool startsOn;
};

constexpr std::array<LineEntry, kStatusLineCount> kStatusLines{
    LineEntry{"Activity", "status_activity", true},
    LineEntry{"Bubble", "status_bubble", true},
    LineEntry{"Slice set", "status_slice_set", true},
    LineEntry{"Closest spawn", "status_closest_spawn", true},
};

[[nodiscard]] constexpr std::array<bool, kOverlayCount> starting_state() noexcept {
    std::array<bool, kOverlayCount> state{};

    for (std::size_t index = 0; index < state.size(); ++index) {
        state[index] = kOverlays[index].startsOn;
    }

    return state;
}

[[nodiscard]] constexpr std::array<bool, kStatusLineCount> starting_line_state() noexcept {
    std::array<bool, kStatusLineCount> state{};

    for (std::size_t index = 0; index < state.size(); ++index) {
        state[index] = kStatusLines[index].startsOn;
    }

    return state;
}

std::array<bool, kOverlayCount> g_enabled{starting_state()};
std::array<bool, kStatusLineCount> g_lineEnabled{starting_line_state()};

[[nodiscard]] bool in_range(Overlay overlay) noexcept {
    return static_cast<std::size_t>(overlay) < kOverlayCount;
}

[[nodiscard]] bool in_range(StatusLine line) noexcept {
    return static_cast<std::size_t>(line) < kStatusLineCount;
}

[[nodiscard]] const char* theme_storage_name() noexcept {
    switch (theme::selected()) {
    case theme::Style::sunriseOriginal:
        return kThemeSunriseOriginal;

    case theme::Style::rgb:
    default:
        return kThemeRgb;
    }
}

[[nodiscard]] std::array<store::Switch, kSwitchCount> switch_state() noexcept {
    std::array<store::Switch, kSwitchCount> switches{};

    for (std::size_t index = 0; index < kOverlayCount; ++index) {
        switches[index] = {kOverlays[index].storageKey, g_enabled[index]};
    }

    for (std::size_t index = 0; index < kStatusLineCount; ++index) {
        switches[kOverlayCount + index] = {kStatusLines[index].storageKey, g_lineEnabled[index]};
    }

    return switches;
}

void save_settings() noexcept {
    const std::array<store::Switch, kSwitchCount> switches = switch_state();
    (void)store::save(switches, theme_storage_name());
}

/**
 * HUD cards use the selected theme's border.
 *
 * RGB deliberately uses the live RGB cycle. Sunrise Original uses the normal ImGui border
 * authored by the Sunrise theme instead of forcing an RGB override.
 */
[[nodiscard]] ImVec4 overlay_border_color() noexcept {
    if (theme::selected() == theme::Style::rgb) {
        return theme::animated_border_color();
    }

    return ImGui::GetStyleColorVec4(ImGuiCol_Border);
}

[[nodiscard]] float draw_overlay(const Entry& entry, const ImVec2& position) noexcept {
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, scaling::dpi::pixels(1.0F));
    ImGui::PushStyleColor(ImGuiCol_Border, overlay_border_color());

    const bool submitContents = ImGui::Begin(entry.windowId, nullptr, kOverlayFlags);

    if (submitContents) {
        entry.draw();
    }

    const float height = ImGui::GetWindowSize().y;

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    return height;
}

} // namespace

void initialize(void* module) noexcept {
    store::initialize(module);

    std::array<store::Switch, kSwitchCount> switches = switch_state();

    /*
     * RGB is the default for a fresh hud.json. An existing saved choice still wins.
     */
    char storedTheme[kThemeStorageCapacity]{};
    (void)std::snprintf(storedTheme, sizeof(storedTheme), "%s", kThemeRgb);


    store::load(switches, storedTheme, sizeof(storedTheme));

    for (std::size_t index = 0; index < kOverlayCount; ++index) {
        g_enabled[index] = switches[index].on;
    }

    for (std::size_t index = 0; index < kStatusLineCount; ++index) {
        g_lineEnabled[index] = switches[kOverlayCount + index].on;
    }

    /*
     * Only the explicit Sunrise Original storage value selects Original.
     * Missing, invalid or newer unknown values safely use the RGB default.
     */
    if (std::strcmp(storedTheme, kThemeSunriseOriginal) == 0) {
        theme::set_selected(theme::Style::sunriseOriginal);
    } else {
        theme::set_selected(theme::Style::rgb);
    }

}

void shutdown() noexcept {
    store::shutdown();
}

const char* display_name(Overlay overlay) noexcept {
    return in_range(overlay) ? kOverlays[static_cast<std::size_t>(overlay)].displayName : "";
}

bool enabled(Overlay overlay) noexcept {
    return in_range(overlay) && g_enabled[static_cast<std::size_t>(overlay)];
}

void set_enabled(Overlay overlay, bool on) noexcept {
    if (!in_range(overlay) || g_enabled[static_cast<std::size_t>(overlay)] == on) {
        return;
    }

    g_enabled[static_cast<std::size_t>(overlay)] = on;
    save_settings();
}

const char* display_name(StatusLine line) noexcept {
    return in_range(line) ? kStatusLines[static_cast<std::size_t>(line)].displayName : "";
}

bool enabled(StatusLine line) noexcept {
    return in_range(line) && g_lineEnabled[static_cast<std::size_t>(line)];
}

void set_enabled(StatusLine line, bool on) noexcept {
    if (!in_range(line) || g_lineEnabled[static_cast<std::size_t>(line)] == on) {
        return;
    }

    g_lineEnabled[static_cast<std::size_t>(line)] = on;
    save_settings();
}

theme::Style selected_theme() noexcept {
    return theme::selected();
}

void set_selected_theme(theme::Style style) noexcept {
    if (theme::selected() == style) {
        return;
    }

    theme::set_selected(style);
    save_settings();
}


bool draw(bool interfaceEnabled) noexcept {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    if (!interfaceEnabled || viewport == nullptr) {
        return false;
    }

    theme::update();

    const float margin = scaling::dpi::pixels(kViewportMargin);
    const float gap = scaling::dpi::pixels(kOverlayGap);

    ImVec2 position{
        viewport->WorkPos.x + margin,
        viewport->WorkPos.y + margin,
    };

    bool drawn = false;

    for (std::size_t index = 0; index < kOverlayCount; ++index) {
        if (!g_enabled[index]) {
            continue;
        }

        position.y += draw_overlay(kOverlays[index], position) + gap;
        drawn = true;
    }

    return drawn;
}

} // namespace sunrise::core::ui::hud
