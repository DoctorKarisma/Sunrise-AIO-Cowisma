#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::state::activity::events {

/**
 * The Tower's seasonal events, as the operator picks them.
 *
 * Each event is a handful of per-bubble roster groups, one registry key per area it dresses.
 * Showing one event is subtractive: the roster withholds every other event's keys. The map below
 * ties each key to its event and area. It was attributed on 2026-09-04 by publishing one carrier
 * per area at a time and walking the Tower.
 */
enum class Event : std::uint8_t {
    festivalOfTheLost,
    dawning,
    ironBanner,
    crimsonDays,
    solstice,
    trialsSaint14,
    count,
};

inline constexpr std::size_t kEventCount = static_cast<std::size_t>(Event::count);

/** Menu label of each event, in `Event` order. */
inline constexpr std::array<const char*, kEventCount> kEventNames = {
    "Festival of the Lost",
    "The Dawning",
    "Iron Banner",
    "Crimson Days",
    "Solstice of Heroes",
    "Trials of Osiris / Saint-14",
};

/** Where each event shows, in `Event` order. */
inline constexpr std::array<const char*, kEventCount> kEventAreas = {
    "Courtyard, Bazaar, Hangar and Annex.",
    "Courtyard, snow in the Bazaar and Hangar, and the Hangar rink with its ball game, goals and "
    "scoreboards. Bungie dressed no Dawning in the Annex. Also dresses the Farm.",
    "Courtyard only.",
    "Courtyard, and the Farm.",
    "Courtyard only.",
    "The Saint-14 ship in the Hangar.",
};

/** One registry key, the event it carries and the area it dresses. */
struct EventKey {
    std::uint32_t key{};
    Event event{};
    const char* area{};
};

/**
 * Every event key the roster may withhold. Four admitted keys are deliberately not here, so the
 * page never withholds them, because their slots name no event: 0x50CC9C7D is the Courtyard
 * vendors' squad group (Banshee, Rahool, the postmaster, Shaxx, Tess, Zavala, Eva), 0x08E64D48 and
 * 0x099B0342 hold bare engagement sensors, and 0x4F4ED92F is the Annex `o_penumbra_vendor` slot
 * (Benedict 99-40, Season of Opulence), the same shape as the Drifter and Ada-1 slots beside it.
 * None of them shows anything on its own.
 */
inline constexpr std::array<EventKey, 14> kEventKeys = {{
    {0x7C6DE64FU, Event::festivalOfTheLost, "Courtyard"},
    {0xFC6B8707U, Event::festivalOfTheLost, "Bazaar"},
    {0xEE34BBABU, Event::festivalOfTheLost, "Hangar"},
    {0x6D3740C6U, Event::festivalOfTheLost, "Annex"},
    {0x00ACD208U, Event::dawning, "Courtyard"},
    {0x2F2B8D00U, Event::dawning, "Bazaar snow"},
    {0x6E087824U, Event::dawning, "Hangar snow"},
    {0xDA989AA3U, Event::dawning, "Hangar rink"},
    {0xC140FF19U, Event::dawning, "Farm"},
    {0x4488AD94U, Event::crimsonDays, "Farm"},
    {0x27060E6CU, Event::ironBanner, "Courtyard"},
    {0x6CEFCC01U, Event::crimsonDays, "Courtyard"},
    {0xD5B68262U, Event::solstice, "Courtyard"},
    {0x9052672CU, Event::trialsSaint14, "Hangar"},
}};

/**
 * The client's own idea of an event is a set of unlock flags, and the family-5 override list is
 * how this server sets them. Each event has one identity flag, its first-year flag, which the
 * Director nodes, Eva's stock and the expression pool's "an event is live" row all read: 777
 * Crimson Days, 779 The Dawning, 884 Solstice of Heroes, 946 Festival of the Lost, 992 Guardian
 * Games. Later years
 * added stock flags (947 and 950 Festival, 7201 Crimson) and one value per event that must read 1
 * (12496 Dawning, 8137 Festival, 8482 Crimson). Iron Banner and the Saint-14 ship are not events
 * to the client, so they set nothing; slot 6038, once mistaken for Iron Banner, is the Forsaken
 * entitlement. Attributed 2026-09-04 from the collectibles and vendor rows that read each slot.
 */
struct EventFlag {
    std::uint16_t slot{};
    Event event{};
};

struct EventValue {
    std::uint16_t slot{};
    std::int32_t value{};
    Event event{};
};

/** Logical flag value the client reads as set. */
inline constexpr std::uint8_t kFlagLive = 2;

inline constexpr std::array<EventFlag, 7> kEventFlags = {{
    {946, Event::festivalOfTheLost},
    {947, Event::festivalOfTheLost},
    {950, Event::festivalOfTheLost},
    {779, Event::dawning},
    {777, Event::crimsonDays},
    {7201, Event::crimsonDays},
    {884, Event::solstice},
}};

/**
 * Guardian Games is a theme without dressing: its Courtyard podium group (key 0x0AFC31B6) is
 * authored but its placement list was emptied in the shipped data, so the roster can show nothing
 * for it. Its identity flag is therefore owned by the music choice alone.
 */
inline constexpr std::uint16_t kGuardianGamesFlag = 992;

inline constexpr std::array<EventValue, 3> kEventValues = {{
    {8137, 1, Event::festivalOfTheLost},
    {12496, 1, Event::dawning},
    {8482, 1, Event::crimsonDays},
}};

/**
 * Which event's theme the Tower plays. Proven 2026-09-05: the theme follows the event's identity
 * flag alone (779 Dawning, 777 Crimson Days, 946 Festival of the Lost, 884 Solstice, 992 Guardian
 * Games), and with several set the client picks one by its own priority. `followEvents` leaves
 * it to that priority; an explicit choice sets only that event's identity flag, so the other shown
 * events keep their dressing, stock flags and live values but lose what their identity flag gates
 * (Eva's Dawning stock, for one). The client reads its flags once at boot, so a change applies at
 * the next launch.
 */
enum class Music : std::uint8_t {
    followEvents,  // the client picks among the shown events' themes; Guardian Games never plays
    festivalOfTheLost,
    dawning,
    crimsonDays,
    solstice,
    guardianGames,
    count,
};

inline constexpr std::size_t kMusicCount = static_cast<std::size_t>(Music::count);

/** Menu label of each choice, in `Music` order. */
inline constexpr std::array<const char*, kMusicCount> kMusicNames = {
    "Follow the shown events",
    "Festival of the Lost",
    "The Dawning",
    "Crimson Days",
    "Solstice of Heroes",
    "Guardian Games",
};

/** The token each choice writes to `event_music.txt`, in `Music` order. */
inline constexpr std::array<const char*, kMusicCount> kMusicTokens = {
    "auto", "festival", "dawning", "crimson", "solstice", "guardian_games",
};

/** @return The identity flag that carries an event's theme, or 0 for an event without one. */
[[nodiscard]] constexpr std::uint16_t identity_flag(Event event) noexcept {
    switch (event) {
    case Event::festivalOfTheLost: return 946;
    case Event::dawning: return 779;
    case Event::crimsonDays: return 777;
    case Event::solstice: return 884;
    default: return 0;
    }
}

/** @return The event whose theme a choice names, or `Event::count` for `followEvents`. */
[[nodiscard]] constexpr Event music_event(Music music) noexcept {
    switch (music) {
    case Music::festivalOfTheLost: return Event::festivalOfTheLost;
    case Music::dawning: return Event::dawning;
    case Music::crimsonDays: return Event::crimsonDays;
    case Music::solstice: return Event::solstice;
    default: return Event::count;
    }
}

/** Room for every mapped key and for ones a hand-edited file names that the map does not. */
inline constexpr std::size_t kKeyCapacity = 64;

/** A small set of registry keys with no duplicates. Zero is not a key. */
struct KeySet {
    std::array<std::uint32_t, kKeyCapacity> keys{};
    std::size_t count{};

    /** @return True when the key is in the set. */
    [[nodiscard]] constexpr bool contains(std::uint32_t key) const noexcept {
        for (std::size_t index = 0; index < count; ++index) {
            if (keys[index] == key) {
                return true;
            }
        }
        return false;
    }

    /** @return True when the key is in the set afterwards. Zero and a full set are refused. */
    constexpr bool insert(std::uint32_t key) noexcept {
        if (key == 0) {
            return false;
        }
        if (contains(key)) {
            return true;
        }
        if (count == keys.size()) {
            return false;
        }
        keys[count] = key;
        ++count;
        return true;
    }

    /** Takes one key out. Order is not kept; nothing reads the set in order. */
    constexpr void erase(std::uint32_t key) noexcept {
        for (std::size_t index = 0; index < count; ++index) {
            if (keys[index] == key) {
                --count;
                keys[index] = keys[count];
                keys[count] = 0;
                return;
            }
        }
    }
};

/** @return True when both sets hold the same keys, in any order. */
[[nodiscard]] constexpr bool same_keys(const KeySet& left, const KeySet& right) noexcept {
    if (left.count != right.count) {
        return false;
    }
    for (std::size_t index = 0; index < left.count; ++index) {
        if (!right.contains(left.keys[index])) {
            return false;
        }
    }
    return true;
}

/**
 * @param withheld Keys the roster withholds.
 * @param event Event asked about.
 * @return True while none of the event's keys are withheld.
 */
[[nodiscard]] constexpr bool shown(const KeySet& withheld, Event event) noexcept {
    for (const EventKey& entry : kEventKeys) {
        if (entry.event == event && withheld.contains(entry.key)) {
            return false;
        }
    }
    return true;
}

/**
 * Takes every key of one event out of the withheld set, or puts them all in. Keys the map does not
 * name are left as they are.
 * @param withheld Keys the roster withholds, updated in place.
 * @param event Event to show or hide.
 * @param visible True to show the event.
 * @return True when every key of the event fitted the set.
 */
constexpr bool show(KeySet& withheld, Event event, bool visible) noexcept {
    bool complete = true;
    for (const EventKey& entry : kEventKeys) {
        if (entry.event != event) {
            continue;
        }
        if (visible) {
            withheld.erase(entry.key);
        } else {
            complete = withheld.insert(entry.key) && complete;
        }
    }
    return complete;
}

/** @param key A key read from the file. @return Its map entry, or null when the map lacks it. */
[[nodiscard]] constexpr const EventKey* find_key(std::uint32_t key) noexcept {
    for (const EventKey& entry : kEventKeys) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace sunrise::state::activity::events
