/** @file */
#pragma once
// Shared constexpr spec for the 'account_currencies' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_currencies/Types.hpp>

namespace rpc::spec::handlers::account_currencies {

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field("account_index", deprecated),
    field("strict", deprecated));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::account_currencies
