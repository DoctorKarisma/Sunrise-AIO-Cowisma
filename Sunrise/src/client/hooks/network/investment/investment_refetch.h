#pragma once

namespace sunrise::client::hooks::network::investment {

/**
 * Binds the client's own opcode-205 request thunk: a parameterless function that builds the
 * empty family-5 status request and hands it to the generic Web Service sender. The catalog names
 * it `queuez_family5_status_commit_request`; it has no code callers, so nothing else can be made
 * to fire it on demand.
 * @return True when the thunk was found.
 */
[[nodiscard]] bool install_refetch() noexcept;

/**
 * Fires the thunk once when State has asked for a client refetch (a music choice or an Events-page
 * save changed the family-5 overrides). Call from the frame poll on the game's own thread: the
 * request must be issued where the client issues its own.
 */
void poll_refetch() noexcept;

} // namespace sunrise::client::hooks::network::investment
