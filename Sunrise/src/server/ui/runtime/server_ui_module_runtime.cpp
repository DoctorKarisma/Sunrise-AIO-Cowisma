#include "server_ui_module_runtime.h"

#include <string_view>

#include "../../../core/ui/modules/registry/ui_module_registry.h"
#include "../../../core/ui/modules/ui_module_descriptor.h"
#include "../activity_host/activity_host_panel.h"
#include "../activity_override/activity_override_panel.h"
#include "../tower_events/tower_events_panel.h"

namespace sunrise::server::ui::runtime {
namespace {

/** A namespaced stable ID keeps Server modules from clashing with Client modules. */
constexpr std::string_view kOverrideStableId = "server.activity_override";
/** Short menu label for the activity override page. */
constexpr std::string_view kOverrideDisplayName = "Activity";

/** A namespaced stable ID for the Activity Host page. */
constexpr std::string_view kHostStableId = "server.activity_host";
/** Short menu label for the Activity Host page. */
constexpr std::string_view kHostDisplayName = "Activity Host";

/** A namespaced stable ID for the Tower Events page. */
constexpr std::string_view kEventsStableId = "server.tower_events";
/** Short menu label for the Tower Events page. */
constexpr std::string_view kEventsDisplayName = "Events";

core::ui::modules::registry::PageRegistration g_overridePage;
core::ui::modules::registry::PageRegistration g_hostPage;
core::ui::modules::registry::PageRegistration g_eventsPage;

} // namespace

/** @return True when the Server module owns all of its Core UI registry slots. */
bool initialize() noexcept {
    if (!g_overridePage.acquire(core::ui::modules::Owner::server,
                                kOverrideStableId,
                                kOverrideDisplayName,
                                &activity_override::draw)) {
        return false;
    }

    if (!g_hostPage.acquire(core::ui::modules::Owner::server,
                            kHostStableId,
                            kHostDisplayName,
                            &activity_host::draw,
                            nullptr,
                            &activity_host::draw_windows)) {
        g_overridePage.release();
        return false;
    }

    if (!g_eventsPage.acquire(core::ui::modules::Owner::server,
                              kEventsStableId,
                              kEventsDisplayName,
                              &tower_events::draw)) {
        g_hostPage.release();
        g_overridePage.release();
        return false;
    }

    return true;
}

/** Removes the Server module's pages from the Core UI registry. */
void shutdown() noexcept {
    g_eventsPage.release();
    g_hostPage.release();
    g_overridePage.release();
}

} // namespace sunrise::server::ui::runtime
