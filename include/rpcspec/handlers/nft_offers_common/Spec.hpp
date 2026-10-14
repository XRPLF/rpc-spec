/** @file */
#pragma once
// Shared constexpr spec for the 'nft_buy_offers' / 'nft_sell_offers' RPC commands.

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/nft_offers_common/Types.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::nft_offers_common {

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("nft_id", &Input::nftID, required, asUint256),
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
        defaultTo(kLimitDefault),
        ifServerClio(clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})),
        asUint32),
    field(
        "marker",
        &Input::marker,
        ifServerXrpld(
            withCustomError(
                type<std::string>,
                rpc::XrpldError::RpcInvalidParams,
                "Invalid field 'marker', not string."),
            withCustomError(uint256Hex, rpc::XrpldError::RpcInvalidParams)),
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

}  // namespace rpc::spec::handlers::nft_offers_common
