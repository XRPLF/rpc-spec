/** @file */
#pragma once

#include <rpcspec/Account.hpp>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_offers {

/**
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, deferredAccountId),
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
    field("marker", &Input::marker, accountMarkerStr),
    field("ledger", ifServerClio(deprecated)),
    field("strict", ifServerClio(deprecated)));

/**
 * @brief The API v2 spec, derived from `kInputSpecV1`.
 */
inline constexpr auto kInputSpecV2 = kInputSpecV1;

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

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

}  // namespace rpc::spec::handlers::account_offers
