/** @file */
#pragma once

#include <rpcspec/Account.hpp>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_currencies/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_currencies {

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "ident",
        &Input::account,
        ifServerXrpld(
            ifAbsent("account"),
            withCustomError(
                required,
                rpc::XrpldError::RpcInvalidParams,
                "Missing field 'account'.")),
        ifServerClio(never),
        deferredAccountId),
    field("account", &Input::account, ifServerClio(required), deferredAccountId),
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
    field("marker", &Input::marker, accountMarkerStrInvalidParams),
    field("account_index", ifServerClio(deprecated)),
    field("strict", ifServerClio(deprecated)));

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

}  // namespace rpc::spec::handlers::account_currencies
