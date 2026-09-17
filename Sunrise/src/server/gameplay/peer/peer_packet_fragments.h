#pragma once

#include <memory>

#include "../../../middleware/gameplay/peer/packet_fragments.h"
#include "../../../state/gameplay/definition.h"
#include "../../../state/gameplay/external/entity_identity.h"

namespace sunrise::server::gameplay::peer {

/** One complete native packet spans at most eight transport fragments. */
struct JoinedPacket final {
    std::array<std::byte, middleware::gameplay::peer::packet_fragments::kMaximumPacketBytes>
        bytes{};
    std::size_t size{};
};

/** Whole datagrams borrow input; fragmented packets retain their assembled bytes and channel. */
struct PacketInput final {
    std::span<const std::byte> payload{};
    std::unique_ptr<JoinedPacket> storage{};
    state::gameplay::entity_identity::Source source{};
};

/** Only complete packets reach the established decoder. */
[[nodiscard]] bool prepare_packet_input(const state::gameplay::Endpoint& from,
                                        std::span<const std::byte> payload,
                                        std::uint64_t now,
                                        PacketInput& output) noexcept;
/** A completed packet cannot cross a channel replacement after reassembly unlocks. */
[[nodiscard]] bool packet_channel_matches(const state::gameplay::PeerLink&,
                                          const PacketInput&) noexcept;

} // namespace sunrise::server::gameplay::peer
