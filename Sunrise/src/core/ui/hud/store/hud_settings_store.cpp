/**
 * The HUD settings store. It is separate from Core settings because the interface changes these
 * values while the game runs and saves each change at once. Core settings are read once.
 */

#include "hud_settings_store.h"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../filesystem/path.h"
#include "../../../logging/log.h"

namespace sunrise::core::ui::hud::store {
namespace {

/** The module-owned HUD settings file, beside the generated settings and logs. */
constexpr std::wstring_view kFileSuffix = L"\\hud.json";

/**
 * Theme settings add only two small rows to the old switch document. 4096 bytes leaves generous
 * room for future HUD switches without making this runtime file meaningfully larger.
 */
constexpr std::size_t kFileCapacity = 4096;

/** 48 bytes fit one quoted overlay key, the same ceiling the module stable IDs use. */
constexpr std::size_t kQuotedKeyCapacity = 48;

/** Theme names are deliberately short stable storage identifiers. */
constexpr std::size_t kThemeValueCapacity = 32;

/** The two values a boolean row can carry. Anything else keeps the caller's default. */
constexpr char kTrueText[] = "true";
constexpr char kFalseText[] = "false";

/** Theme and animation keys live beside the existing overlay switches. */
constexpr char kThemeKey[] = "theme";
constexpr char kAnimatedKey[] = "animated";

path::Buffer g_path{};
bool g_pathResolved{};

/** @param reason Key naming the step that failed. */
void report_fail(const char* reason) noexcept {
    std::array<char, 96> line{};
    const int written =
        std::snprintf(line.data(), line.size(), "ev=hud stage=store result=fail reason=%s", reason);

    if (written > 0) {
        log::write(
            log::Channel::core, log::Level::warn, {line.data(), static_cast<std::size_t>(written)});
    }
}

/**
 * Finds the beginning of a value belonging to one JSON key.
 *
 * This store intentionally remains a tiny reader for its own generated flat document rather than
 * becoming a general JSON parser.
 */
[[nodiscard]] std::size_t value_begin(std::string_view text, const char* key) noexcept {
    std::array<char, kQuotedKeyCapacity> quoted{};

    const int length = std::snprintf(quoted.data(), quoted.size(), "\"%s\"", key);
    if (length <= 0 || static_cast<std::size_t>(length) >= quoted.size()) {
        return std::string_view::npos;
    }

    const std::size_t at =
        text.find(std::string_view(quoted.data(), static_cast<std::size_t>(length)));

    if (at == std::string_view::npos) {
        return std::string_view::npos;
    }

    const std::size_t colon = text.find(':', at);
    if (colon == std::string_view::npos) {
        return std::string_view::npos;
    }

    std::size_t begin = colon + 1;

    while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }

    return begin;
}

/**
 * Finds one key and reads the boolean value after it.
 *
 * @param text Whole document.
 * @param key Bare key.
 * @param output Receives the value, untouched when the key or value is absent.
 */
void bool_for(std::string_view text, const char* key, bool& output) noexcept {
    const std::size_t begin = value_begin(text, key);

    if (begin == std::string_view::npos) {
        return;
    }

    const std::string_view value = text.substr(begin);

    if (value.starts_with(kTrueText)) {
        output = true;
    } else if (value.starts_with(kFalseText)) {
        output = false;
    }
}

/**
 * Reads one quoted string value.
 *
 * @param text Whole document.
 * @param key Bare key.
 * @param output Destination buffer.
 * @param outputCapacity Destination capacity including its terminator.
 */
void string_for(std::string_view text,
                const char* key,
                char* output,
                std::size_t outputCapacity) noexcept {
    if (output == nullptr || outputCapacity == 0) {
        return;
    }

    const std::size_t begin = value_begin(text, key);

    if (begin == std::string_view::npos || begin >= text.size() || text[begin] != '"') {
        return;
    }

    const std::size_t valueBegin = begin + 1;
    const std::size_t valueEnd = text.find('"', valueBegin);

    if (valueEnd == std::string_view::npos || valueEnd < valueBegin) {
        return;
    }

    const std::size_t length = valueEnd - valueBegin;

    if (length == 0 || length >= outputCapacity) {
        return;
    }

    std::memcpy(output, text.data() + valueBegin, length);
    output[length] = '\0';
}

/**
 * Appends one formatted boolean row.
 *
 * @param document Whole document buffer.
 * @param offset Bytes already written, advanced by the appended row.
 * @param key Row key.
 * @param value Row value.
 * @param last True when this is the final document row.
 */
[[nodiscard]] bool append_bool_row(std::array<char, kFileCapacity>& document,
                                   int& offset,
                                   const char* key,
                                   bool value,
                                   bool last) noexcept {
    const auto used = static_cast<std::size_t>(offset);

    if (used >= document.size()) {
        return false;
    }

    const int written = std::snprintf(document.data() + offset,
                                      document.size() - used,
                                      "  \"%s\": %s%s\n",
                                      key,
                                      value ? kTrueText : kFalseText,
                                      last ? "" : ",");

    if (written <= 0 || static_cast<std::size_t>(written) >= document.size() - used) {
        return false;
    }

    offset += written;
    return true;
}

/**
 * Appends one quoted string row.
 *
 * Theme storage names are internal fixed identifiers and therefore contain no characters that
 * require JSON escaping.
 */
[[nodiscard]] bool append_string_row(std::array<char, kFileCapacity>& document,
                                     int& offset,
                                     const char* key,
                                     const char* value,
                                     bool last) noexcept {
    if (value == nullptr) {
        return false;
    }

    const auto used = static_cast<std::size_t>(offset);

    if (used >= document.size()) {
        return false;
    }

    const int written = std::snprintf(document.data() + offset,
                                      document.size() - used,
                                      "  \"%s\": \"%s\"%s\n",
                                      key,
                                      value,
                                      last ? "" : ",");

    if (written <= 0 || static_cast<std::size_t>(written) >= document.size() - used) {
        return false;
    }

    offset += written;
    return true;
}

} // namespace

/** Resolves the HUD settings file. It reads nothing; load does that. */
void initialize(void* module) noexcept {
    g_path = path::Buffer{};
    g_pathResolved = path::artifact_directory(module, g_path) && path::append(g_path, kFileSuffix);

    if (!g_pathResolved) {
        report_fail("path");
    }
}

/** Drops the resolved file path. */
void shutdown() noexcept {
    g_path = path::Buffer{};
    g_pathResolved = false;
}

/** Applies saved HUD state over the caller's defaults. */
void load(std::span<Switch> switches,
          char* theme,
          std::size_t themeCapacity,
          bool& animated) noexcept {
    if (!g_pathResolved) {
        return;
    }

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

    const std::string_view text(buffer.data(), read);

    // Missing or malformed keys retain their caller-provided defaults.
    for (Switch& entry : switches) {
        bool_for(text, entry.key, entry.on);
    }

    string_for(text, kThemeKey, theme, themeCapacity);
    bool_for(text, kAnimatedKey, animated);
}

/** Writes the complete HUD settings file. */
bool save(std::span<const Switch> switches, const char* theme, bool animated) noexcept {
    if (!g_pathResolved || theme == nullptr) {
        return false;
    }

    std::array<char, kFileCapacity> document{};

    int offset = std::snprintf(document.data(), document.size(), "{\n");
    if (offset <= 0) {
        return false;
    }

    if (!append_string_row(document, offset, kThemeKey, theme, false)) {
        report_fail("capacity");
        return false;
    }

    const bool noSwitches = switches.empty();

    if (!append_bool_row(document, offset, kAnimatedKey, animated, noSwitches)) {
        report_fail("capacity");
        return false;
    }

    for (std::size_t index = 0; index < switches.size(); ++index) {
        const Switch& entry = switches[index];

        if (!append_bool_row(document, offset, entry.key, entry.on, index + 1 == switches.size())) {
            report_fail("capacity");
            return false;
        }
    }

    const auto used = static_cast<std::size_t>(offset);

    if (used >= document.size()) {
        report_fail("capacity");
        return false;
    }

    const int closed = std::snprintf(document.data() + offset, document.size() - used, "}\n");

    if (closed <= 0 || static_cast<std::size_t>(closed) >= document.size() - used) {
        report_fail("capacity");
        return false;
    }

    offset += closed;

    const HANDLE file = CreateFileW(g_path.chars.data(),
                                    GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        report_fail("open");
        return false;
    }

    DWORD written = 0;
    const auto size = static_cast<DWORD>(offset);

    bool complete =
        WriteFile(file, document.data(), size, &written, nullptr) != FALSE && written == size;

    complete = CloseHandle(file) != FALSE && complete;

    if (!complete) {
        report_fail("write");
    }

    return complete;
}

} // namespace sunrise::core::ui::hud::store
