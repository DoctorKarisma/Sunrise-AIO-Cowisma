#pragma once

#include <cstdint>

#include "definition.h"

namespace sunrise::state::activity::events {

/**
 * The withheld-key set is kept twice: the set the roster publishes from, and the set the file
 * `roster_exclude_keys.txt` beside settings.json holds. The Events page writes the file. A fresh
 * activity join copies the file into the published set, and nothing else does. That join is the
 * one moment the client rebuilds its roster from nothing, so a change never lands under a client
 * standing in the Tower; going to orbit and back is how one is applied.
 *
 * The file stays the source of truth so the presets in `event_presets` and hand edits work the
 * same way the page does. One hex key per line, `0x` optional, `#` comments. No file, or an empty
 * one, withholds nothing: every event shows.
 *
 * The selection also owns the events' unlock flags in the family-5 override list (see
 * `kEventFlags`): a shown event's flags are set live and a hidden event's are cleared, whatever
 * settings.json authored for those slots. That happens from the pending set as soon as it is
 * known, because the client asks for its investment object when a Tower load begins, before the
 * join that copies the pending roster set into the published one.
 */

/** Re-reads the file into both sets. Called on a fresh activity join, and once on first use. */
void reload() noexcept;

/**
 * Loads the file if nothing has yet. The family-5 responders call this first, so the very first
 * investment object the client receives already carries the selection's event flags.
 */
void ensure_loaded() noexcept;

/**
 * @param key Registry key the roster is about to publish.
 * @return True when the published set withholds it.
 */
[[nodiscard]] bool withheld(std::uint32_t key) noexcept;

/**
 * @param event Event to query.
 * @return True when the published Tower selection currently shows that event.
 *
 * Uses the published selection rather than the pending menu state, so mission scripting always
 * agrees with the roster belonging to the current Tower join. A menu change takes effect after
 * leaving the Tower and loading it again, just like the event dressing.
 */
[[nodiscard]] bool enabled(Event event) noexcept;

/**
 * Copies both sets.
 * @param published Keys the roster withholds now.
 * @param pending Keys the file holds, which the next join will withhold.
 */
void snapshot(KeySet& published, KeySet& pending) noexcept;

/**
 * Rewrites the file from one set and makes it the pending set.
 * @param pending Keys the next join is to withhold.
 * @return True when every byte reached the file.
 */
[[nodiscard]] bool save(const KeySet& pending) noexcept;

/** @return Which theme the Tower is asked to play; `event_music.txt` beside settings.json. */
[[nodiscard]] Music music() noexcept;

/**
 * Writes the music choice and re-applies the event flags. Heard at the next launch.
 * @return True when the file was written.
 */
[[nodiscard]] bool set_music(Music music) noexcept;

} // namespace sunrise::state::activity::events
