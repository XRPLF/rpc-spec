/** @file */
#pragma once
// Shared constexpr spec for the 'transaction_entry' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/transaction_entry/Types.hpp>

namespace rpc::spec::handlers::transaction_entry {

inline constexpr auto kSpec = RpcSpec{
    field(
        "tx_hash",
        withCustomError(required, ClioError::RpcFieldNotFoundTransaction),
        uint256Hex
    ),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
};

} // namespace rpc::spec::handlers::transaction_entry
