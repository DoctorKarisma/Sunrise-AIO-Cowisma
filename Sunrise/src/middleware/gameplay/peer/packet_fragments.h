#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::middleware::gameplay::peer::packet_fragments {

/** Direct IPv4 transport admits 1,228 bytes per datagram. */
inline constexpr std::size_t kDefaultDatagramLimit = 1228;
/** The native transport's largest datagram buffer holds 1,240 bytes. */
inline constexpr std::size_t kMaximumDatagramLimit = 1240;
/** Fragment headers contain two bytes before their payload. */
inline constexpr std::size_t kFragmentHeaderBytes = 2;
/** Three-bit fragment indices and counts admit eight pieces per packet. */
inline constexpr std::size_t kMaximumFragments = 8;
/** The native receive ring retains eight fragment groups. */
inline constexpr std::size_t kMaximumGroups = 8;
/** One group stores eight maximum-sized fragment bodies. */
inline constexpr std::size_t kMaximumPacketBytes =
    kMaximumFragments * (kMaximumDatagramLimit - kFragmentHeaderBytes);
/** A matching sequence starts a new group only after more than 2,000 milliseconds. */
inline constexpr std::uint64_t kExpiryMilliseconds = 2000;

enum class Result : std::uint8_t { refused, incomplete, duplicate, complete };

/** Each caller-owned store belongs to one authenticated peer channel and needs about 80 KiB. */
class Store final {
public:
    /**
     * Returns a complete original packet without altering its inner header.
     * @param datagram One decrypted transport datagram.
     * @param expectedGuard The admitted connection sequence modulo four.
     * @param nowMs Unwrapped monotonic arrival time in milliseconds.
     * @param output Receives the packet only on complete; must not overlap this store.
     * @param written Cleared, then receives the complete packet's byte count.
     * @param datagramLimit This channel's maximum datagram size, including the fragment header.
     * @return Complete only once for a retained group; all other results leave output unchanged.
     */
    [[nodiscard]] Result accept(std::span<const std::byte> datagram,
                                std::uint8_t expectedGuard,
                                std::uint64_t nowMs,
                                std::span<std::byte> output,
                                std::size_t& written,
                                std::size_t datagramLimit = kDefaultDatagramLimit) noexcept;

    /** Reset on channel replacement; payload storage needs no clearing before reuse. */
    void reset() noexcept;

private:
    struct Group final {
        std::array<std::byte, kMaximumPacketBytes> bytes{};
        std::uint64_t startedMs{};
        std::size_t bodyLimit{}, finalBytes{};
        std::uint8_t sequence{}, guard{}, count{}, present{};
        bool active{};
    };

    std::array<Group, kMaximumGroups> groups_{};
    std::size_t nextGroup_{};
};

} // namespace sunrise::middleware::gameplay::peer::packet_fragments
