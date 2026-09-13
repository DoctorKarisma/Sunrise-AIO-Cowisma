#pragma once

#include "../../../../middleware/bap/activity_message/activity_message_request_parser.h"
#include "../internal.h"

namespace sunrise::server::bap::encrypted::festival_pickups {

/**
 * Validates one authored Festival of the Lost pickup and queues its Candy reward through
 * Cow's existing world-reward queue. Called with the BAP route lock held.
 */
void receive(const ActivityClientBinding& binding,
             const middleware::bap::activity_message::Request& request) noexcept;

} // namespace sunrise::server::bap::encrypted::festival_pickups
