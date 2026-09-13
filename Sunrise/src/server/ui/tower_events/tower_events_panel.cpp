/**
 * The Tower events module's interface. Every toggle saves the file at once. What the roster
 * publishes moves only on the next activity join, so the page shows both: what is dressing the
 * Tower now, and what the next load will dress it with.
 */

#include "tower_events_panel.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <imgui.h>

#include "../../../core/ui/components/label/ui_label_component.h"
#include "../../../core/ui/components/section/ui_section_component.h"
#include "../../../core/ui/components/toggle/ui_toggle_component.h"
#include "../../../state/activity/events/activity_event_selection.h"

namespace sunrise::server::ui::tower_events {
namespace {

namespace selection = state::activity::events;
namespace label = core::ui::components::label;

/** Room for a status line naming every event. */
constexpr std::size_t kStatusCapacity = 256;

/** Set when the last save did not reach the file, cleared by the next one that does. */
bool g_saveFailed{};
/** Same, for the music choice. */
bool g_musicFailed{};

/** Draws one wrapped line in the muted text colour. */
void muted(const char* text) noexcept {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

/**
 * Names the events a set leaves showing.
 * @param withheld Keys withheld.
 * @param output Receives "all events", "no events", or the names, comma-separated.
 */
void describe(const selection::KeySet& withheld,
              std::array<char, kStatusCapacity>& output) noexcept {
    int used = 0;
    std::size_t showing = 0;
    for (std::size_t index = 0; index < selection::kEventCount; ++index) {
        if (!selection::shown(withheld, static_cast<selection::Event>(index))) {
            continue;
        }
        ++showing;
        if (used < 0 || static_cast<std::size_t>(used) >= output.size()) {
            continue;
        }
        used += std::snprintf(output.data() + used,
                              output.size() - static_cast<std::size_t>(used),
                              "%s%s",
                              used == 0 ? "" : ", ",
                              selection::kEventNames[index]);
    }
    if (showing == selection::kEventCount) {
        (void)std::snprintf(output.data(), output.size(), "all events");
    } else if (showing == 0) {
        (void)std::snprintf(output.data(), output.size(), "no events");
    }
}

/**
 * Shows what the Tower is dressed with now and, when it differs, what the next load brings.
 * @param published Keys the roster withholds now.
 * @param pending Keys the file holds.
 */
void draw_status(const selection::KeySet& published, const selection::KeySet& pending) noexcept {
    std::array<char, kStatusCapacity> now{};
    describe(published, now);
    ImGui::TextUnformatted("Showing now");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", now.data());
    if (selection::same_keys(published, pending)) {
        muted("The selection is in effect.");
    } else {
        std::array<char, kStatusCapacity> next{};
        describe(pending, next);
        ImGui::TextUnformatted("Next Tower load");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", next.data());
        muted("Go to orbit and load back into the Tower to apply it.");
    }
    if (g_saveFailed) {
        ImGui::TextUnformatted("roster_exclude_keys.txt could not be written, nothing was saved");
    }
}

/** Shows or hides every event at once. @param visible True to show them all. */
void show_all(selection::KeySet& pending, bool visible) noexcept {
    for (std::size_t index = 0; index < selection::kEventCount; ++index) {
        (void)selection::show(pending, static_cast<selection::Event>(index), visible);
    }
}

} // namespace

/** Draws the Tower events module inside the active Core UI frame. */
void draw() noexcept {
    selection::KeySet published{};
    selection::KeySet pending{};
    selection::snapshot(published, pending);

    core::ui::components::section::header(
        "Tower events",
        "Choose which seasonal events dress the Tower. A change applies the next time you load "
        "into the Tower: go to orbit and come back in.");

    bool changed = false;
    for (std::size_t index = 0; index < selection::kEventCount; ++index) {
        const auto event = static_cast<selection::Event>(index);
        bool visible = selection::shown(pending, event);
        if (core::ui::components::toggle::control(selection::kEventNames[index], visible)) {
            (void)selection::show(pending, event, visible);
            changed = true;
        }
        label::align();
        muted(selection::kEventAreas[index]);
        ImGui::Spacing();
    }

    if (ImGui::Button("Show all")) {
        show_all(pending, true);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide all")) {
        show_all(pending, false);
        changed = true;
    }

    if (changed) {
        g_saveFailed = !selection::save(pending);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Tower music");
    int music = static_cast<int>(selection::music());
    ImGui::SetNextItemWidth(260.0F);
    if (ImGui::Combo("##tower_music",
                     &music,
                     selection::kMusicNames.data(),
                     static_cast<int>(selection::kMusicCount))) {
        g_musicFailed = !selection::set_music(static_cast<selection::Music>(music));
    }
    label::align();
    muted("Which event's theme plays. With several events shown the client picks one on its own; "
          "choose one here to decide. Heard on the next load into a destination: go to orbit and come back in.");
    if (g_musicFailed) {
        ImGui::TextUnformatted("event_music.txt could not be written, nothing was saved");
    }

    ImGui::Spacing();
    ImGui::Separator();
    draw_status(published, pending);
}

} // namespace sunrise::server::ui::tower_events
