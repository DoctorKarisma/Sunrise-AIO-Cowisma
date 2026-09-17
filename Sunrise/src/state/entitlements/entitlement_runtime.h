#pragma once

#include "definition.h"

namespace sunrise::state::entitlements {

/** @return A call-local ownership table from the saved account. */
[[nodiscard]] Table get() noexcept;
[[nodiscard]] bool snapshot(Table& output) noexcept;

} // namespace sunrise::state::entitlements
