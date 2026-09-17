#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "../../../state/account/inventory/seen_state.h"
#include "../web_service_envelope.h"

namespace sunrise::middleware::web_service::messages::opcode702 {

/** Web Service opcode of the character object B write-back. */
inline constexpr std::uint16_t kOpcode = 702;
/** The full 5360-byte character mirror packs into at most 4800 bytes. */
inline constexpr std::size_t kPayloadSize = 4800;
/** Value of the world-state field once the client has entered the world. */
inline constexpr std::uint8_t kInWorld = 8;

/** Supported fields from the character writeback. */
struct Request {
    /**
     * Five-bit field at objB `+12068`, schema path `.0.11.1.0.0.4`. Measured 0 on the orbit
     * screen, 1 from the launch through the load, 8 after `activity:in_world`.
     */
    std::uint8_t worldState{};
    bool hasWorldState{};
    std::optional<state::account::inventory::CharacterNewItems> newItems;
};

/**
 * Reads the complete character writeback with optional groups and bounded zero padding.
 * @param message Parsed Web Service envelope.
 * @param request Receives only fields present in a valid body.
 * @return False for malformed or truncated fields.
 */
[[nodiscard]] bool parse_request(const Message& message, Request& request) noexcept;

} // namespace sunrise::middleware::web_service::messages::opcode702
