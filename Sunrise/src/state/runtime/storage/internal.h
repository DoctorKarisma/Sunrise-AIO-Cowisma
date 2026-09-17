#pragma once

#include <Windows.h>

#include "../state.h"

namespace sunrise::state::runtime::storage {

/** Root process-local session State; progression lives in the investment store. */
extern State g_state;
/** Windows reader-writer lock protecting mutable root State fields. */
extern SRWLOCK g_stateLock;

} // namespace sunrise::state::runtime::storage
