/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_nfts/Types.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::account_nfts {

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field(
        "limit",
        &Input::limit,
        ifServerXrpld(nullAs(kLimitDefault)),
        ifServerXrpld(
            withCustomError(
                type<uint32_t>,
                rpc::XrpldError::RpcInvalidParams,
                "Invalid field 'limit', not unsigned integer."),
            withCustomError(
                min(uint32_t{1}),
                rpc::XrpldError::RpcInvalidParams,
                "Invalid field 'limit'.")),
        ifServerClio(type<uint32_t>, min(uint32_t{1})),
        ifServerClio(clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})),
        defaultTo(kLimitDefault),
        asUint32),
    field(
        "marker",
        &Input::marker,
        ifServerXrpld(withCustomError(
            type<std::string>,
            rpc::XrpldError::RpcInvalidParams,
            "Invalid field 'marker', not string.")),
        asUint256));

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

}  // namespace rpc::spec::handlers::account_nfts
