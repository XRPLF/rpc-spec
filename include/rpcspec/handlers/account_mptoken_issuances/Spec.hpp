/** @file */
#pragma once
// Shared constexpr spec for the 'account_mptoken_issuances' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_mptoken_issuances/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_mptoken_issuances {

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountIdActMalformed),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32),
    field("marker", &Input::marker, accountMarker, asString),
    field("ledger", deprecated));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::account_mptoken_issuances
