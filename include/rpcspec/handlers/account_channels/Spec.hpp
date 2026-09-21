/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_channels/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_channels {

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field("destination_account", &Input::destinationAccount, type<std::string>, accountId),
    field("limit", &Input::limit)                          //
        | type<uint32_t>                                   //
        | min(uint32_t{1})                                 //
        | defaultTo(kLimitDefault)                         //
        | clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})  //
        | asUint32,
    field("marker", &Input::marker, accountMarker, asString));

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/**
 * @brief ADL hook: resolve the versioned spec from the Input type.
 *
 * @return A reference to this handler's `kSpec`, for `HandlerFor` to select a version from.
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::account_channels
