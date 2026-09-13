#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace sunrise::middleware::bap::activity_message::loot_pickup {

/** Global SObject 3539, schema 0x808087F0, in build 86657. */
inline constexpr std::uint32_t kIncidentTarget = 3539;

struct Pickup {
    std::uint32_t nonce{};
    std::uint64_t accountSoid{};
    std::uint64_t characterSoid{};
    std::uint32_t sourceHash{};
    std::array<float, 3> position{};
    std::int32_t bubble{-1};
};

/** Decodes the placed-loot form, rejecting unsupported subjects and noncanonical padding. */
[[nodiscard]] bool parse(std::span<const std::byte> payload, Pickup& output) noexcept;

} // namespace sunrise::middleware::bap::activity_message::loot_pickup
