/** @file */
#pragma once
// Shared constexpr spec for the 'transaction_entry' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/transaction_entry/Types.hpp>

namespace rpc::spec::handlers::transaction_entry {

inline constexpr auto kInputSpec = spec<Input>(
    field(
        "tx_hash",
        &Input::txHash,
        withCustomError(required, ClioError::RpcFieldNotFoundTransaction),
        ledgerHashHex
    ),
    field("ledger_hash", &Input::ledgerHash, ledgerHashHex),
    field("ledger_index", &Input::ledgerIndex, ledgerIndexOpt)
);

inline constexpr auto& kSpec = kInputSpec;

} // namespace rpc::spec::handlers::transaction_entry
