#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../socket_entry_lists/definition.h"

namespace sunrise::state::build_data::socket_entry_buckets {

/** No shipped subclass carries more socket-entry lists with a super lane than this. */
inline constexpr std::size_t kDefinitionCapacity = 32;
/** Marks an entry whose selector chain never reaches one of the 12 semantic ability buckets. */
inline constexpr std::uint8_t kNoDestinationBucket = 0xFF;
/**
 * One socket-entry list's resolved ability-bucket destination per entry.
 * A pick's table position does not say which slot it fills; only its selector chain does. Computed
 * once during content extraction so runtime code can route a click without re-reading content.
 */
struct Definition {
    std::uint16_t socketEntryListIndex{};
    std::array<std::uint8_t, socket_entry_lists::kEntryCapacity> buckets{};
};

} // namespace sunrise::state::build_data::socket_entry_buckets

