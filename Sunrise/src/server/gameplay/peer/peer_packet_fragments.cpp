#include "peer_packet_fragments.h"

#include <new>

#include "../../../middleware/gameplay/peer/established_packet.h"
#include "../gameplay_log.h"
#include "peer_transport_internal.h"

namespace sunrise::server::gameplay::peer {
namespace {
namespace gp = state::gameplay;
namespace wire = middleware::gameplay::peer;
namespace fragments = wire::packet_fragments;
/** The second packet bit selects the outer fragmentation header. */
constexpr std::byte kFragmentedPacketBit{0x40};

/** Reassembly belongs to one exact peer channel, independently of its external views. */
struct FragmentChannel final {
    fragments::Store packets{};
    std::uint64_t peerGeneration{}, channelGeneration{};
    std::uint32_t remoteConnectionSequence{}, localConnectionSequence{};
};
std::array<FragmentChannel, gp::kAssociationCapacity> g_fragmentChannels{};

} // namespace

/**
 * Reassembles only authenticated fragments from the currently bound channel.
 * @param from Peer endpoint including the local listening port.
 * @param payload One decrypted fragment.
 * @param now Arrival time in milliseconds.
 * @param output Receives the whole original packet on completion.
 * @param source Captures the channel that supplied the completed packet.
 * @return True only when every piece and the inner guard agree.
 */
bool join_packet_fragments(const gp::Endpoint& from,
                           std::span<const std::byte> payload,
                           std::uint64_t now,
                           JoinedPacket& output,
                           gp::entity_identity::Source& source) noexcept {
    fragments::Result result = fragments::Result::refused;
    AcquireSRWLockExclusive(&g_lock);
    gp::PeerLink* const peer = find_locked(from);
    if (peer != nullptr && peer->stage >= gp::PeerStage::connecting && peer->peerGeneration != 0
        && peer->channelGeneration != 0) {
        auto& channel = g_fragmentChannels[static_cast<std::size_t>(peer - g_peers.data())];
        if (channel.peerGeneration != peer->peerGeneration
            || channel.channelGeneration != peer->channelGeneration
            || channel.remoteConnectionSequence != peer->remoteConnectionSequence
            || channel.localConnectionSequence != peer->localConnectionSequence) {
            channel.packets.reset();
            channel.peerGeneration = peer->peerGeneration;
            channel.channelGeneration = peer->channelGeneration;
            channel.remoteConnectionSequence = peer->remoteConnectionSequence;
            channel.localConnectionSequence = peer->localConnectionSequence;
        }
        result =
            channel.packets.accept(payload,
                                   wire::connection_sequence_low2(peer->remoteConnectionSequence),
                                   now,
                                   output.bytes,
                                   output.size);
        source = entity_source(*peer);
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (result == fragments::Result::complete) {
        report(core::log::Level::debug,
               "ev=gameplay stage=packet_reassembly result=complete bytes=%zu",
               output.size);
    } else if (result == fragments::Result::refused) {
        report(core::log::Level::debug,
               "ev=gameplay stage=packet_reassembly result=refused bytes=%zu",
               payload.size());
    }
    return result == fragments::Result::complete;
}

/**
 * Borrows ordinary input and assembles fragmented input outside the caller stack.
 * @param from Authenticated peer endpoint.
 * @param payload Decrypted transport datagram.
 * @param now Arrival time in milliseconds.
 * @param output Retains the complete packet and its channel identity.
 * @return False for incomplete, duplicate, or invalid fragments.
 */
bool prepare_packet_input(const gp::Endpoint& from,
                          std::span<const std::byte> payload,
                          std::uint64_t now,
                          PacketInput& output) noexcept {
    output = {};
    if (payload.empty()) {
        return false;
    }
    if ((payload.front() & kFragmentedPacketBit) == std::byte{}) {
        output.payload = payload;
        return true;
    }
    output.storage.reset(new (std::nothrow) JoinedPacket{});
    if (!output.storage
        || !join_packet_fragments(from, payload, now, *output.storage, output.source)) {
        return false;
    }
    output.payload = std::span(output.storage->bytes).first(output.storage->size);
    return true;
}

/** A guard-bit wrap does not make a replaced channel the source of an old packet. */
bool packet_channel_matches(const gp::PeerLink& peer, const PacketInput& input) noexcept {
    return !input.storage
           || (peer.peerGeneration == input.source.peerGeneration
               && peer.channelGeneration == input.source.channelGeneration
               && peer.remoteConnectionSequence == input.source.remoteConnectionSequence
               && peer.localConnectionSequence == input.source.localConnectionSequence);
}

/** Releases every retained fragment when the peer transport resets. */
void reset_packet_assemblies() noexcept {
    for (auto& channel : g_fragmentChannels) {
        channel.packets.reset();
        channel.peerGeneration = 0;
        channel.channelGeneration = 0;
        channel.remoteConnectionSequence = 0;
        channel.localConnectionSequence = 0;
    }
}

} // namespace sunrise::server::gameplay::peer
