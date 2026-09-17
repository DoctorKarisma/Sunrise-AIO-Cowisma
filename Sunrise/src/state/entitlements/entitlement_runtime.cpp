#include "entitlement_runtime.h"

#include "../investment/store.h"
#include "validation.h"

namespace sunrise::state::entitlements {

/** @return A call-local ownership table from the saved account. */
Table get() noexcept {
    Table table;
    (void)snapshot(table);
    return table;
}

/** An unavailable ownership table must not become a successful partial response. */
bool snapshot(Table& output) noexcept {
    if (!investment::store::read_entitlements(output) || !valid(output)) {
        output = {};
        return false;
    }
    return true;
}

} // namespace sunrise::state::entitlements
