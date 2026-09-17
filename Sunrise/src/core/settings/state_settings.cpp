#include "parser.h"

namespace sunrise::core::settings::parser {

/** Parses local activity policy; account preferences belong to the save. */
bool Parser::state_settings(Settings& output) noexcept {
    if (!consume('{')) {
        return false;
    }
    if (consume('}')) {
        return true;
    }
    for (;;) {
        std::string_view key;
        if (!string(key) || !consume(':')) {
            return false;
        }
        if (key == "activity") {
            if (!activity_settings(output.initialActivityDefaults)) {
                return false;
            }
        } else if (!skip_value(0)) {
            return false;
        }
        if (consume('}')) {
            return true;
        }
        if (!consume(',')) {
            return false;
        }
    }
}

} // namespace sunrise::core::settings::parser
