#include "packet_fragments.h"

#include <cstring>

namespace sunrise::middleware::gameplay::peer::packet_fragments {
namespace {

/** MSB-first headers begin with the marker bit, then the fragmentation bit. */
constexpr unsigned kMarkerMask = 0x80, kFragmentedMask = 0x40;
/** The first fragment-header byte ends with the six-bit group sequence. */
constexpr unsigned kSequenceMask = 0x3F;
/** The second fragment-header byte carries guard, count-minus-one, then index. */
constexpr unsigned kOuterGuardShift = 6, kCountShift = 3, kIndexMask = 7;
/** Whole packet headers carry the two-bit connection guard immediately after the flag bits. */
constexpr unsigned kInnerGuardShift = 4, kGuardMask = 3;

/** The original packet header must already identify this channel and an unfragmented packet. */
bool valid_packet(std::span<const std::byte> bytes, std::uint8_t guard) noexcept {
    if (bytes.empty()) {
        return false;
    }
    const auto first = std::to_integer<unsigned>(bytes.front());
    return (first & (kMarkerMask | kFragmentedMask)) == 0
           && ((first >> kInnerGuardShift) & kGuardMask) == guard;
}

} // namespace

/**
 * Buffers one bounded fragment and publishes only a complete packet with both guards intact.
 * @param datagram One decrypted transport datagram.
 * @param expectedGuard The admitted connection sequence modulo four.
 * @param nowMs Unwrapped monotonic arrival time in milliseconds.
 * @param output Receives the complete packet and may alias datagram, but not this store.
 * @param written Cleared unless complete is returned.
 * @param datagramLimit Maximum datagram bytes on this channel.
 * @return Refused leaves prior fragments intact; duplicate indices never overwrite payload.
 */
Result Store::accept(std::span<const std::byte> datagram,
                     std::uint8_t expectedGuard,
                     std::uint64_t nowMs,
                     std::span<std::byte> output,
                     std::size_t& written,
                     std::size_t datagramLimit) noexcept {
    written = 0;
    if (expectedGuard > kGuardMask || datagramLimit <= kFragmentHeaderBytes
        || datagramLimit > kMaximumDatagramLimit || datagram.empty()
        || datagram.size() > datagramLimit) {
        return Result::refused;
    }
    const auto first = std::to_integer<unsigned>(datagram.front());
    if ((first & kMarkerMask) != 0) {
        return Result::refused;
    }
    if ((first & kFragmentedMask) == 0) {
        if (!valid_packet(datagram, expectedGuard) || output.size() < datagram.size()) {
            return Result::refused;
        }
        std::memmove(output.data(), datagram.data(), datagram.size());
        written = datagram.size();
        return Result::complete;
    }
    if (datagram.size() < kFragmentHeaderBytes) {
        return Result::refused;
    }
    const auto second = std::to_integer<unsigned>(datagram[1]);
    const auto guard = static_cast<std::uint8_t>(second >> kOuterGuardShift);
    const auto count = static_cast<std::uint8_t>(((second >> kCountShift) & kIndexMask) + 1U);
    const auto index = static_cast<std::uint8_t>(second & kIndexMask);
    const auto sequence = static_cast<std::uint8_t>(first & kSequenceMask);
    const auto body = datagram.subspan(kFragmentHeaderBytes);
    const auto bodyLimit = datagramLimit - kFragmentHeaderBytes;
    if (guard != expectedGuard || index >= count
        || (index + 1U != count && body.size() != bodyLimit)
        || (index == 0 && !valid_packet(body, expectedGuard))
        || (count == 1 && output.size() < body.size())) {
        return Result::refused;
    }

    Group* selected = nullptr;
    for (auto& group : groups_) {
        if (group.active && group.sequence == sequence) {
            selected = &group;
            break;
        }
    }
    if (selected == nullptr) {
        selected = &groups_[nextGroup_];
        nextGroup_ = (nextGroup_ + 1U) % groups_.size();
        selected->active = false;
    } else if (nowMs >= selected->startedMs && nowMs - selected->startedMs > kExpiryMilliseconds) {
        selected->active = false;
    }
    auto& group = *selected;
    if (!group.active) {
        group.startedMs = nowMs;
        group.bodyLimit = bodyLimit;
        group.finalBytes = 0;
        group.sequence = sequence;
        group.guard = guard;
        group.count = count;
        group.present = 0;
        group.active = true;
    } else if (group.count != count || group.guard != guard || group.bodyLimit != bodyLimit) {
        return Result::refused;
    }

    const auto fragmentBit = static_cast<std::uint8_t>(1U << index);
    if ((group.present & fragmentBit) != 0) {
        return Result::duplicate;
    }
    const auto received = static_cast<std::uint8_t>(group.present | fragmentBit);
    const auto complete = static_cast<std::uint8_t>((1U << count) - 1U);
    const auto finalBytes = index + 1U == count ? body.size() : group.finalBytes;
    const auto packetBytes = (count - 1U) * bodyLimit + finalBytes;
    // A caller with too little space can retry the missing piece without losing the prior group.
    if (received == complete && output.size() < packetBytes) {
        return Result::refused;
    }
    if (!body.empty()) {
        std::memcpy(group.bytes.data() + index * bodyLimit, body.data(), body.size());
    }
    group.finalBytes = finalBytes;
    group.present = received;
    if (received != complete) {
        return Result::incomplete;
    }
    const auto packet = std::span<const std::byte>(group.bytes).first(packetBytes);
    if (!valid_packet(packet, expectedGuard)) {
        return Result::refused;
    }
    std::memmove(output.data(), packet.data(), packet.size());
    written = packet.size();
    return Result::complete;
}

/** Withdraws every sequence without materializing or clearing the payload arrays. */
void Store::reset() noexcept {
    for (auto& group : groups_) {
        group.active = false;
        group.present = 0;
    }
    nextGroup_ = 0;
}

} // namespace sunrise::middleware::gameplay::peer::packet_fragments
