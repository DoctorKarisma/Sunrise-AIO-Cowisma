#include "server_http.h"

#include <cstdint>
#include <string_view>

#include "../../core/logging/log.h"
#include "../../core/runtime/server_clock.h"
#include "../../middleware/signon/response.h"
#include "../../state/entitlements/entitlement_runtime.h"
#include "../../state/runtime/runtime.h"

namespace sunrise::server::http {
namespace {

/** URL marker owned by the in-process SignOn route. Any query string may follow it. */
constexpr std::string_view kSignOnPath = "/SignOn";
/** HTTP success status written to the Client result prefix. */
constexpr unsigned kHttpOk = 200;
/** Network-order IPv4 loopback. The route answers in process, so the client is always local. */
constexpr std::uint32_t kObservedClientAddress = 0x7F000001;

} // namespace

/** Encodes the generated SignOn state for the supported HTTP route. */
bool consume(const client::network::HttpRequest& request,
             client::network::HttpResponse& response) noexcept {
    if (request.url.find(kSignOnPath) == std::string_view::npos) {
        return false;
    }
    const auto& signOnState = state::sign_on();
    const auto serverTime = static_cast<std::uint64_t>(core::runtime::server_clock_seconds());
    const std::uint64_t expiry = serverTime + signOnState.tokenLifetimeSeconds;
    state::entitlements::Table ownership;
    if (!state::entitlements::snapshot(ownership)
        || !middleware::signon::encode_success(signOnState,
                                               ownership,
                                               expiry,
                                               serverTime,
                                               kObservedClientAddress,
                                               request.response,
                                               response.size)) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::warn,
                         "ev=http method=post route=signon stage=encode result=fail");
        return false;
    }
    response.statusCode = kHttpOk;
    // The answered sign-on moment is the account's session clock, and the character records
    // publish it as the last reset before sign-in.
    state::publish_sign_in_time(serverTime);
    core::log::write(core::log::Channel::server,
                     core::log::Level::info,
                     "ev=http method=post route=signon result=ok");
    return true;
}

} // namespace sunrise::server::http
