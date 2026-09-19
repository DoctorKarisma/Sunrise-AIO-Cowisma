#include "server_ui_module_runtime.h"

#include <string_view>

#include "../../../core/ui/modules/registry/ui_module_registry.h"
#include "../../../core/ui/modules/ui_module_descriptor.h"
#include "../activity_host/activity_host_panel.h"
#include "../spawn/spawn_panel.h"
#include "../tower_events/tower_events_panel.h"
#include "../weapon_editor/weapon_editor_panel.h"

namespace sunrise::server::ui::runtime {
namespace {

/** A namespaced stable ID for the Activity Host page. */
constexpr std::string_view kHostStableId = "server.activity_host";
/** Short menu label for the Activity Host page. */
constexpr std::string_view kHostDisplayName = "Activity Host";

/** A namespaced stable ID for the Tower Events page. */
constexpr std::string_view kEventsStableId = "server.tower_events";
/** Short menu label for the Tower Events page. */
constexpr std::string_view kEventsDisplayName = "Events";

/** A namespaced stable ID for the Gear Editor page. */
constexpr std::string_view kWeaponEditorStableId = "server.weapon_editor";
/** Short menu label for the Gear Editor page. */
constexpr std::string_view kWeaponEditorDisplayName = "Gear Editor";

/** A namespaced stable ID for the Spawn page. */
constexpr std::string_view kSpawnStableId = "server.spawn";
/** Short menu label for the Spawn page. */
constexpr std::string_view kSpawnDisplayName = "Spawn";

core::ui::modules::registry::PageRegistration g_hostPage;
core::ui::modules::registry::PageRegistration g_eventsPage;
core::ui::modules::registry::PageRegistration g_weaponEditorPage;
core::ui::modules::registry::PageRegistration g_spawnPage;

} // namespace

/** @return True when the Server module owns all of its Core UI registry slots. */
bool initialize() noexcept {
    if (!g_hostPage.acquire(core::ui::modules::Owner::server,
                            kHostStableId,
                            kHostDisplayName,
                            &activity_host::draw,
                            nullptr,
                            &activity_host::draw_windows)) {
        return false;
    }

    if (!g_eventsPage.acquire(core::ui::modules::Owner::server,
                              kEventsStableId,
                              kEventsDisplayName,
                              &tower_events::draw)) {
        g_hostPage.release();
        return false;
    }

    if (!g_weaponEditorPage.acquire(core::ui::modules::Owner::server,
                                    kWeaponEditorStableId,
                                    kWeaponEditorDisplayName,
                                    &weapon_editor::draw)) {
        g_eventsPage.release();
        g_hostPage.release();
        return false;
    }

    if (!g_spawnPage.acquire(
            core::ui::modules::Owner::server, kSpawnStableId, kSpawnDisplayName, &spawn::draw)) {
        g_weaponEditorPage.release();
        g_eventsPage.release();
        g_hostPage.release();
        return false;
    }

    return true;
}

/** Removes the Server module's pages from the Core UI registry. */
void shutdown() noexcept {
    g_spawnPage.release();
    g_weaponEditorPage.release();
    g_eventsPage.release();
    g_hostPage.release();
}

} // namespace sunrise::server::ui::runtime
