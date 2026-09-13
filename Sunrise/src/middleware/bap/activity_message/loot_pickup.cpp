#include "loot_pickup.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "../../encoding/bit_reader.h"

namespace sunrise::middleware::bap::activity_message::loot_pickup {

bool parse(std::span<const std::byte> payload, Pickup& output) noexcept {
    output = {};

    if (payload.size() != 80 && payload.size() != 82) {
        return false;
    }

    encoding::bits::Reader reader(payload);
    const auto expect = [&reader](std::uint8_t width, std::uint64_t expected) noexcept {
        std::uint64_t value{};
        return reader.read(width, value) && value == expected;
    };

    Pickup parsed{};
    std::uint64_t value{};

    if (!reader.read(32, value)) {
        return false;
    }
    parsed.nonce = static_cast<std::uint32_t>(value);

    if (!expect(3, 2) || !reader.read(64, parsed.characterSoid) || !expect(6, 18)) {
        return false;
    }

    std::uint64_t subjectFlags{};
    if (!reader.read(5, subjectFlags)) {
        return false;
    }

    const bool legacy = payload.size() == 80 && subjectFlags == 0;
    const bool cow = payload.size() == 82 && subjectFlags == 0x10;
    if (!legacy && !cow) {
        return false;
    }

    // Cow's placed-loot form carries one additional 16-bit subject field when bit 0x10 is set.
    if (cow) {
        std::uint64_t extra{};
        if (!reader.read(16, extra) || extra != 0x0F00) {
            return false;
        }
    }

    if (!expect(32, 0x811C9DC5U) || !expect(32, 0x811C9DC5U) || !expect(32, 0x811C9DC5U)
        || !expect(64, parsed.characterSoid) || !expect(6, 3)
        || !reader.read(64, parsed.accountSoid) || !expect(3, 1) || !expect(2, 1)
        || !expect(64, parsed.characterSoid) || !expect(32, 0x80000001U)
        || !reader.read(32, value)) {
        return false;
    }

    parsed.sourceHash = static_cast<std::uint32_t>(value);

    for (float& coordinate : parsed.position) {
        if (!reader.read(32, value)) {
            return false;
        }
        coordinate = std::bit_cast<float>(static_cast<std::uint32_t>(value));
        if (!std::isfinite(coordinate)) {
            return false;
        }
    }

    if (!reader.read(32, value)) {
        return false;
    }
    parsed.bubble = static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x80000000LL);

    if (!expect(32, 0x811C9DC5U) || !expect(7, 0) || reader.remaining_bits() != 0
        || parsed.accountSoid == 0 || parsed.characterSoid == 0) {
        return false;
    }

    output = parsed;
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::loot_pickup
