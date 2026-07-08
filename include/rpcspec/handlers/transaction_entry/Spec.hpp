/** @file */
#pragma once
// Shared constexpr spec for the 'transaction_entry' RPC command.
// Single source of truth — both Clio and xrpld include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/transaction_entry/Types.hpp>

namespace rpc::spec::handlers::transaction_entry {

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "tx_hash",
        &Input::txHash,
        withCustomError(required, ClioError::RpcFieldNotFoundTransaction),
        asUint256));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::transaction_entry
