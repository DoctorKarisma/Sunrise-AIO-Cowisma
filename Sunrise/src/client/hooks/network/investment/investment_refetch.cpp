/**
 * On-demand family-5 refetch.
 *
 * The client reads its account unlock overrides from the family-5 object it fetches with Web
 * Service opcode 205, and it fetches that object only at sign-in and at the end of a loading
 * stage. A flag the server changes afterwards - the Tower music choice, an Events-page save -
 * never reaches a client that is already in the world. The client's own request thunk for that
 * opcode is a parameterless function (RVA 0xF2BA00 in this build); calling it from the frame poll
 * makes the client ask again, and the family-5 commit that follows re-arms the derived rebuild.
 */

#include "investment_refetch.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <string_view>

#include "../../../../core/logging/log.h"
#include "../../../../state/investment/investment_overrides.h"
#include "internal.h"

namespace sunrise::client::hooks::network::investment {
namespace {

/**
 * The opcode-205 request thunk. The immediate 0xCD is the opcode; the two calls before it build
 * the empty payload and fetch the sender context. Unique in the image.
 */
constexpr std::string_view kRefetchSignatureText =
    "48 83 EC 38 48 8D 4C 24 40 E8 ? ? ? ? 83 CA FF 48 8D 4C 24 48 E8 ? ? ? ? 4C 8B 4C 24 48 "
    "4C 8D 44 24 40 B9 CD 00 00 00";
constexpr auto kRefetchSignature =
    signature<signature_length(kRefetchSignatureText)>(kRefetchSignatureText);

/**
 * The opcode-503 bootstrap request thunk, the same shape with the opcode 0x1F7. The client issues
 * it before its first world load; firing it after a flag change mirrors the boot the seasonal theme
 * demonstrably follows.
 */
constexpr std::string_view kBootstrapSignatureText =
    "48 83 EC 78 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 68 48 8D 54 24 38 E8 ? ? ? ? 83 CA FF "
    "48 8D 4C 24 30 E8 ? ? ? ? 4C 8B 4C 24 30 4C 8D 44 24 38 B9 F7 01 00 00";
constexpr auto kBootstrapSignature =
    signature<signature_length(kBootstrapSignatureText)>(kBootstrapSignatureText);

using RequestFamily5 = void (*)() noexcept;

std::atomic<RequestFamily5> g_request{nullptr};
std::atomic<std::uint32_t> g_sent{0};
/**
 * After a change, keep re-arming the derived-state rebuild for a while. A rebuild notifies the
 * world's unlock listeners, and a change made in orbit has no world to notify: the next world to
 * load would otherwise start from stale listener state (Farm -> orbit -> change -> Tower, 2026-09-05).
 */
constexpr std::uint32_t kRearmWindowFrames = 90000;
constexpr std::uint32_t kRearmEveryFrames = 900;
std::atomic<std::uint32_t> g_rearmFramesLeft{0};

void report(const char* result, const char* reason) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=investment stage=refetch result=%s reason=%s sent=%u",
                                      result,
                                      reason,
                                      g_sent.load(std::memory_order_relaxed));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** Calls the thunk under a handler, so a wrong site cannot take the frame down. */
bool fire(RequestFamily5 request) noexcept {
    __try {
        request();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

bool install_refetch() noexcept {
    if (g_request.load(std::memory_order_acquire) != nullptr) {
        return true;
    }
    std::byte* const target = scan_main_image_unique(kRefetchSignature, "family5_refetch");
    if (target == nullptr) {
        report("fail", "target");
        return false;
    }
    g_request.store(reinterpret_cast<RequestFamily5>(target), std::memory_order_release);
    report("ok", "bound");
    return true;
}

void poll_refetch() noexcept {
    const std::uint32_t left = g_rearmFramesLeft.load(std::memory_order_acquire);
    if (left != 0) {
        g_rearmFramesLeft.store(left - 1, std::memory_order_release);
        if (left % kRearmEveryFrames == 0) {
            arm_derived_rebuild();
        }
    }
    if (!state::investment::consume_client_refetch()) {
        return;
    }
    const RequestFamily5 request = g_request.load(std::memory_order_acquire);
    if (request == nullptr) {
        report("fail", "unbound");
        return;
    }
    if (fire(request)) {
        g_sent.fetch_add(1, std::memory_order_relaxed);
        report("sent", "state");
        g_rearmFramesLeft.store(kRearmWindowFrames, std::memory_order_release);
    } else {
        report("fail", "fault");
    }
}

} // namespace sunrise::client::hooks::network::investment
