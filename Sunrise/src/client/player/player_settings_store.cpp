/**
 * The player configuration store. Separate from Core settings because the interface changes these
 * values while the game runs and saves each one at once. Core settings are read once.
 */

#include "player_settings_store.h"

#include <Windows.h>

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "../../core/filesystem/path.h"
#include "../../core/logging/log.h"

namespace sunrise::client::player {
namespace {

/** The module-owned configuration file, beside the generated settings and logs. */
constexpr std::wstring_view kFileSuffix = L"\\player.json";

/** The document is a few scalars, so one small buffer covers both reading and writing. */
constexpr std::size_t kFileCapacity = 768;

SRWLOCK g_lock{SRWLOCK_INIT};
Settings g_settings{};
core::path::Buffer g_path{};
bool g_pathResolved{};

/** @param reason Key naming the step that failed. */
void report_fail(const char* reason) noexcept {
    std::array<char, 96> line{};

    const int written = std::snprintf(
        line.data(), line.size(), "ev=player stage=store result=fail reason=%s", reason);

    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/**
 * Finds one key and reads the boolean after it.
 * @param text Whole document.
 * @param key Quoted key to locate.
 * @param output Receives the value, untouched when the key is absent.
 */
void boolean_for(std::string_view text, std::string_view key, bool& output) noexcept {

    const std::size_t at = text.find(key);

    if (at == std::string_view::npos) {
        return;
    }

    const std::size_t colon = text.find(':', at + key.size());

    if (colon == std::string_view::npos) {
        return;
    }

    std::size_t begin = colon + 1;

    while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }

    output = text.substr(begin).starts_with("true");
}

/**
 * Finds one signed 32-bit integer after a key.
 * @param text Whole document.
 * @param key Quoted key to locate.
 * @param output Receives the value only when it parses cleanly.
 */
void integer_for(std::string_view text, std::string_view key, std::int32_t& output) noexcept {

    const std::size_t at = text.find(key);

    if (at == std::string_view::npos) {
        return;
    }

    const std::size_t colon = text.find(':', at + key.size());

    if (colon == std::string_view::npos) {
        return;
    }

    std::size_t begin = colon + 1;

    while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }

    const char* const first = text.data() + begin;

    const char* const last = text.data() + text.size();

    std::int32_t value = 0;

    const auto result = std::from_chars(first, last, value);

    if (result.ec != std::errc{}) {
        return;
    }

    if (value < kMinimumFieldOfView || value > kMaximumFieldOfView) {
        return;
    }

    output = value;
}

/**
 * Finds one float after a key.
 * The settings document is tiny, so a bounded temporary buffer keeps parsing simple.
 */
void float_for(std::string_view text, std::string_view key, float& output) noexcept {

    const std::size_t at = text.find(key);

    if (at == std::string_view::npos) {
        return;
    }

    const std::size_t colon = text.find(':', at + key.size());

    if (colon == std::string_view::npos) {
        return;
    }

    std::size_t begin = colon + 1;

    while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }

    std::array<char, 64> number{};
    std::size_t length = 0;

    while (begin < text.size() && length + 1 < number.size() && text[begin] != ','
           && text[begin] != '}' && text[begin] != '\r' && text[begin] != '\n') {

        number[length++] = text[begin++];
    }

    if (length == 0) {
        return;
    }

    char* end = nullptr;

    const float value = std::strtof(number.data(), &end);

    if (end == number.data() || !std::isfinite(value) || value < kMinimumPlayerScale
        || value > kMaximumPlayerScale) {
        return;
    }

    output = value;
}

/**
 * Layers one document over the defaults.
 *
 * Player Size is deliberately session-only. We do NOT load
 * player_scale_enabled or player_scale from player.json.
 *
 * Every launch therefore starts with:
 *
 *     playerScaleEnabled = false
 *     playerScale        = 1.0
 */
void parse(std::string_view text, Settings& output) noexcept {

    boolean_for(text, "\"infinite_ammo_enabled\"", output.infiniteAmmoEnabled);

    boolean_for(text, "\"anti_afk_enabled\"", output.antiAfkEnabled);

    boolean_for(text, "\"field_of_view_override_enabled\"", output.fieldOfViewOverrideEnabled);

    integer_for(text, "\"field_of_view\"", output.fieldOfView);

    /*
     * Intentionally DO NOT load:
     *
     *   "player_scale_enabled"
     *   "player_scale"
     *
     * Player Size always begins disabled at 1.00x.
     */

    output.playerScaleEnabled = false;
    output.playerScale = kDefaultPlayerScale;

    boolean_for(text, "\"no_turnback_enabled\"", output.noTurnbackEnabled);

    boolean_for(text, "\"godmode_enabled\"", output.godmodeEnabled);
}

/**
 * Writes the whole document.
 * @return True when every byte reached the file.
 */
[[nodiscard]] bool store(const Settings& settings) noexcept {

    if (!g_pathResolved) {
        return false;
    }

    std::array<char, kFileCapacity> document{};

    const int size = std::snprintf(document.data(),
                                   document.size(),
                                   "{\n"
                                   "  \"infinite_ammo_enabled\": %s,\n"
                                   "  \"anti_afk_enabled\": %s,\n"
                                   "  \"field_of_view_override_enabled\": %s,\n"
                                   "  \"field_of_view\": %d,\n"
                                   "  \"player_scale_enabled\": %s,\n"
                                   "  \"player_scale\": %.3f,\n"
                                   "  \"no_turnback_enabled\": %s,\n"
                                   "  \"godmode_enabled\": %s\n"
                                   "}\n",
                                   settings.infiniteAmmoEnabled ? "true" : "false",
                                   settings.antiAfkEnabled ? "true" : "false",
                                   settings.fieldOfViewOverrideEnabled ? "true" : "false",
                                   static_cast<int>(settings.fieldOfView),
                                   settings.playerScaleEnabled ? "true" : "false",
                                   static_cast<double>(settings.playerScale),
                                   settings.noTurnbackEnabled ? "true" : "false",
                                   settings.godmodeEnabled ? "true" : "false");

    if (size <= 0 || static_cast<std::size_t>(size) >= document.size()) {
        return false;
    }

    const HANDLE file = CreateFileW(g_path.chars.data(),
                                    GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;

    bool complete =
        WriteFile(file, document.data(), static_cast<DWORD>(size), &written, nullptr) != FALSE
        && written == static_cast<DWORD>(size);

    complete = CloseHandle(file) != FALSE && complete;

    return complete;
}

/** Reads the configuration file into the active settings when one exists. */
void load() noexcept {

    const HANDLE file = CreateFileW(g_path.chars.data(),
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    std::array<char, kFileCapacity> buffer{};

    DWORD read = 0;

    const bool readOk =
        ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &read, nullptr)
        != FALSE;

    (void)CloseHandle(file);

    if (!readOk || read == 0) {
        return;
    }

    parse(std::string_view(buffer.data(), read), g_settings);
}

} // namespace

/** Resolves the configuration file and loads it when one exists. */
void initialize(void* module) noexcept {

    AcquireSRWLockExclusive(&g_lock);

    g_settings = Settings{};

    g_pathResolved =
        core::path::artifact_directory(module, g_path) && core::path::append(g_path, kFileSuffix);

    if (g_pathResolved) {

        load();

        /*
         * Player Size is session-only.
         *
         * parse() already forces these values, but explicitly
         * setting them here makes the startup contract clear.
         */
        g_settings.playerScaleEnabled = false;
        g_settings.playerScale = kDefaultPlayerScale;

        /*
         * Immediately clean any values left by the previous
         * build/session from player.json.
         */
        if (!store(g_settings)) {
            report_fail("startup_reset_write");
        }

    } else {

        report_fail("path");
    }

    ReleaseSRWLockExclusive(&g_lock);
}

/**
 * Resets Player Size before dropping the runtime configuration.
 *
 * Other player settings retain their current values on disk.
 */
void shutdown() noexcept {

    AcquireSRWLockExclusive(&g_lock);

    if (g_pathResolved) {

        /*
         * Player Size must never persist into another session.
         */
        g_settings.playerScaleEnabled = false;
        g_settings.playerScale = kDefaultPlayerScale;

        if (!store(g_settings)) {
            report_fail("shutdown_reset_write");
        }
    }

    g_settings = Settings{};
    g_path = core::path::Buffer{};
    g_pathResolved = false;

    ReleaseSRWLockExclusive(&g_lock);
}

/** @return One lock-consistent copy of the current configuration. */
Settings get() noexcept {

    AcquireSRWLockShared(&g_lock);

    const Settings snapshot = g_settings;

    ReleaseSRWLockShared(&g_lock);

    return snapshot;
}

/** Publishes one configuration and writes it straight to disk. */
bool publish(const Settings& settings) noexcept {

    if (settings.fieldOfView < kMinimumFieldOfView || settings.fieldOfView > kMaximumFieldOfView) {
        return false;
    }

    if (!std::isfinite(settings.playerScale) || settings.playerScale < kMinimumPlayerScale
        || settings.playerScale > kMaximumPlayerScale) {
        return false;
    }

    AcquireSRWLockExclusive(&g_lock);

    g_settings = settings;

    const bool stored = store(settings);

    ReleaseSRWLockExclusive(&g_lock);

    if (!stored) {
        report_fail("write");
    }

    return true;
}

} // namespace sunrise::client::player
