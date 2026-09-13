#include "activity_event_selection.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../core/settings/rule_text.h"
#include "../../investment/investment_overrides.h"

namespace sunrise::state::activity::events {
namespace {

/** The file, beside settings.json. */
constexpr std::wstring_view kFileName = L"roster_exclude_keys.txt";
/** The music choice, beside it: one token from `kMusicTokens`, `#` comments. Absent means auto. */
constexpr std::wstring_view kMusicFileName = L"event_music.txt";
/** Room for the header, every mapped key with its comment, and any unmapped keys after them. */
constexpr std::size_t kDocumentCapacity = 8192;
/** What the page writes above the keys, so a hand editor knows what the file does. */
constexpr char kHeader[] =
    "# Tower events. Every key listed here is WITHHELD from the roster, so the event it carries is "
    "not shown.\n"
    "# Written by the Events page of the Sunrise menu. Hand edits and the presets in event_presets "
    "work the same way.\n"
    "# A change applies the next time the client loads into the Tower (orbit and back), never "
    "while standing in it.\n";

SRWLOCK g_lock = SRWLOCK_INIT;
/** Keys the roster withholds now. */
KeySet g_published{};
/** Keys the file holds. */
KeySet g_pending{};
/** Which theme is asked for. */
Music g_music{Music::followEvents};
bool g_loaded{};

/** Reads the music file. An absent file, or one naming nothing known, is `followEvents`. */
Music read_music_file() noexcept {
    static std::array<char, 256> text{};
    text.fill('\0');
    if (!core::path::read_artifact_text(kMusicFileName, text)) {
        return Music::followEvents;
    }
    // First word that is not inside a comment.
    std::size_t at = 0;
    while (at < text.size() && text[at] != '\0') {
        if (text[at] == '#') {
            while (at < text.size() && text[at] != '\0' && text[at] != '\n') {
                ++at;
            }
            continue;
        }
        if (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n') {
            ++at;
            continue;
        }
        std::size_t end = at;
        while (end < text.size() && text[end] != '\0' && text[end] != ' ' && text[end] != '\t'
               && text[end] != '\r' && text[end] != '\n' && text[end] != '#') {
            ++end;
        }
        const std::string_view word{text.data() + at, end - at};
        for (std::size_t index = 0; index < kMusicCount; ++index) {
            if (word == kMusicTokens[index]) {
                return static_cast<Music>(index);
            }
        }
        return Music::followEvents;
    }
    return Music::followEvents;
}

/** Logs one set. @param stage Which set moved and why. */
void report(const char* stage, const KeySet& keys) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    int used = std::snprintf(
        line.data(), line.size(), "ev=events stage=%s withheld=%zu", stage, keys.count);
    for (std::size_t index = 0;
         index < keys.count && used > 0 && static_cast<std::size_t>(used) < line.size();
         ++index) {
        used += std::snprintf(line.data() + used,
                              line.size() - static_cast<std::size_t>(used),
                              " 0x%08X",
                              keys.keys[index]);
    }
    if (used > 0) {
        const std::size_t length = (std::min)(static_cast<std::size_t>(used), line.size() - 1);
        core::log::write(
            core::log::Channel::state, core::log::Level::info, {line.data(), length});
    }
}

/** Reads the file into one set. An absent or empty file is an empty set. */
void read_file(KeySet& keys) noexcept {
    keys = {};
    static std::array<char, core::rule_text::kRuleTextCapacity> text{};
    if (!core::path::read_artifact_text(kFileName, text)) {
        // Fresh install: no authored selection exists yet. Default to showing no seasonal
        // dressing by withholding every mapped event key. Once the Events page saves a file,
        // that file remains authoritative; an existing header-only file still means "show all".
        for (const EventKey& entry : kEventKeys) {
            (void)keys.insert(entry.key);
        }
        return;
    }
    // The cursor reads bare hex, so a `0x` prefix that starts a field is blanked out first; a bare
    // `0` would otherwise be read and dropped as key zero.
    for (std::size_t at = 0; at + 1 < text.size() && text[at] != '\0'; ++at) {
        const bool prefix = text[at] == '0' && (text[at + 1] == 'x' || text[at + 1] == 'X');
        const bool starts = at == 0 || text[at - 1] == ' ' || text[at - 1] == '\t'
                            || text[at - 1] == '\n' || text[at - 1] == '\r';
        if (prefix && starts) {
            text[at] = ' ';
            text[at + 1] = ' ';
        }
    }
    core::rule_text::Cursor rules{text.data()};
    while (rules.seek_field()) {
        // Zero and anything past the capacity are dropped. The field is stepped past either way.
        (void)keys.insert(rules.read_hex());
    }
}

/** Loads the files into both sets and the music choice. The caller holds the lock exclusively. */
void load_locked(const char* stage) noexcept {
    read_file(g_pending);
    g_published = g_pending;
    g_music = read_music_file();
    g_loaded = true;
    report(stage, g_published);
}

/**
 * Makes the family-5 override list agree with one selection: a shown event's flags and values go
 * live, a hidden event's are cleared. With an explicit music choice the identity flags follow the
 * choice instead, because the theme is what they select. Called with no lock held, because the
 * override list has its own lock and the roster asks this module questions from under other locks.
 * @param pending The set the next join will withhold.
 * @param music The theme asked for.
 */
void apply_flags(const KeySet& pending, Music music) noexcept {
    std::size_t set = 0;
    std::size_t cleared = 0;
    const Event chosen = music_event(music);
    for (const EventFlag& entry : kEventFlags) {
        const bool identity = entry.slot == identity_flag(entry.event);
        const bool live = identity && chosen != Event::count ? entry.event == chosen
                                                              : shown(pending, entry.event);
        if (live) {
            set += investment::set_flag_override(entry.slot, kFlagLive) ? 1 : 0;
        } else {
            investment::clear_flag_override(entry.slot);
            ++cleared;
        }
    }
    // Guardian Games has no dressing, so its flag follows the music choice and nothing else.
    if (music == Music::guardianGames) {
        set += investment::set_flag_override(kGuardianGamesFlag, kFlagLive) ? 1 : 0;
    } else {
        investment::clear_flag_override(kGuardianGamesFlag);
        ++cleared;
    }
    for (const EventValue& entry : kEventValues) {
        if (shown(pending, entry.event)) {
            set += investment::set_value_override(entry.slot, entry.value) ? 1 : 0;
        } else {
            investment::clear_value_override(entry.slot);
            ++cleared;
        }
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int used = std::snprintf(line.data(),
                                   line.size(),
                                   "ev=events stage=flags set=%zu cleared=%zu music=%s",
                                   set,
                                   cleared,
                                   kMusicTokens[static_cast<std::size_t>(music)]);
    if (used > 0) {
        const std::size_t length = (std::min)(static_cast<std::size_t>(used), line.size() - 1);
        core::log::write(
            core::log::Channel::state, core::log::Level::info, {line.data(), length});
    }
}

} // namespace

/** Loads the file the first time anything asks, so a roster built before any join is right. */
void ensure_loaded() noexcept {
    AcquireSRWLockShared(&g_lock);
    const bool loaded = g_loaded;
    ReleaseSRWLockShared(&g_lock);
    if (loaded) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    const bool first = !g_loaded;
    if (first) {
        load_locked("first");
    }
    const KeySet pending = g_pending;
    const Music music = g_music;
    ReleaseSRWLockExclusive(&g_lock);
    if (first) {
        apply_flags(pending, music);
    }
}

namespace {

/**
 * Appends one key line.
 * @param document Text being built.
 * @param used Bytes written so far, advanced.
 * @param key Key to write.
 * @param entry Its map entry, or null for a key the map does not name.
 * @return True when the line fitted.
 */
[[nodiscard]] bool append_key(std::array<char, kDocumentCapacity>& document,
                              int& used,
                              std::uint32_t key,
                              const EventKey* entry) noexcept {
    if (used < 0 || static_cast<std::size_t>(used) >= document.size()) {
        return false;
    }
    const std::size_t left = document.size() - static_cast<std::size_t>(used);
    const int written =
        entry != nullptr
            ? std::snprintf(document.data() + used,
                            left,
                            "0x%08X   # %s (%s)\n",
                            key,
                            kEventNames[static_cast<std::size_t>(entry->event)],
                            entry->area)
            : std::snprintf(document.data() + used, left, "0x%08X   # not in the key map\n", key);
    if (written <= 0 || static_cast<std::size_t>(written) >= left) {
        return false;
    }
    used += written;
    return true;
}

} // namespace

/** Re-reads the file into both sets. */
void reload() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    load_locked("join");
    const KeySet pending = g_pending;
    const Music music = g_music;
    ReleaseSRWLockExclusive(&g_lock);
    apply_flags(pending, music);
}

/** @return True when the published set withholds the key. */
bool withheld(std::uint32_t key) noexcept {
    ensure_loaded();
    AcquireSRWLockShared(&g_lock);
    const bool result = g_published.contains(key);
    ReleaseSRWLockShared(&g_lock);
    return result;
}

/** @return True when the published Tower selection currently shows one event. */
bool enabled(Event event) noexcept {
    ensure_loaded();
    if (event == Event::count) {
        return false;
    }

    AcquireSRWLockShared(&g_lock);
    const KeySet published = g_published;
    ReleaseSRWLockShared(&g_lock);

    return shown(published, event);
}

/** Copies both sets. */
void snapshot(KeySet& published, KeySet& pending) noexcept {
    ensure_loaded();
    AcquireSRWLockShared(&g_lock);
    published = g_published;
    pending = g_pending;
    ReleaseSRWLockShared(&g_lock);
}

/** Rewrites the file from one set and makes it the pending set. */
bool save(const KeySet& pending) noexcept {
    ensure_loaded();
    std::array<char, kDocumentCapacity> document{};
    int used = std::snprintf(document.data(), document.size(), "%s", kHeader);
    if (used <= 0) {
        return false;
    }
    // Mapped keys go out in map order, which groups each event's areas together. Whatever else the
    // set holds follows, so a hand-added key survives a visit to the page.
    bool complete = true;
    for (const EventKey& entry : kEventKeys) {
        if (pending.contains(entry.key)) {
            complete = append_key(document, used, entry.key, &entry) && complete;
        }
    }
    for (std::size_t index = 0; index < pending.count; ++index) {
        const std::uint32_t key = pending.keys[index];
        if (find_key(key) == nullptr) {
            complete = append_key(document, used, key, nullptr) && complete;
        }
    }
    if (!complete
        || !core::path::write_artifact_text(
            kFileName, std::string_view{document.data(), static_cast<std::size_t>(used)})) {
        core::log::write(core::log::Channel::state,
                         core::log::Level::warn,
                         "ev=events stage=save result=failed");
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    g_pending = pending;
    const Music music = g_music;
    ReleaseSRWLockExclusive(&g_lock);
    report("save", pending);
    apply_flags(pending, music);
    investment::request_client_refetch();
    return true;
}

/** @return Which theme the Tower is asked to play. */
Music music() noexcept {
    ensure_loaded();
    AcquireSRWLockShared(&g_lock);
    const Music result = g_music;
    ReleaseSRWLockShared(&g_lock);
    return result;
}

/** Writes the music choice and re-applies the event flags. */
bool set_music(Music music) noexcept {
    ensure_loaded();
    if (static_cast<std::size_t>(music) >= kMusicCount) {
        return false;
    }
    std::array<char, 512> document{};
    const int used = std::snprintf(
        document.data(),
        document.size(),
        "# Tower music. One of: auto festival dawning crimson solstice guardian_games.\n"
        "# auto lets the client pick among the shown events' themes. Heard at the next launch.\n"
        "%s\n",
        kMusicTokens[static_cast<std::size_t>(music)]);
    if (used <= 0
        || !core::path::write_artifact_text(
            kMusicFileName, std::string_view{document.data(), static_cast<std::size_t>(used)})) {
        core::log::write(core::log::Channel::state,
                         core::log::Level::warn,
                         "ev=events stage=music result=failed");
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    g_music = music;
    const KeySet pending = g_pending;
    ReleaseSRWLockExclusive(&g_lock);
    apply_flags(pending, music);
    investment::request_client_refetch();
    return true;
}

} // namespace sunrise::state::activity::events
