/** @file */
#pragma once
// Shared constexpr spec for the 'deposit_authorized' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/deposit_authorized/Types.hpp>

namespace rpc::spec::handlers::deposit_authorized {

inline constexpr auto kSpec = RpcSpec{
    field("source_account", required, account),
    field("destination_account", required, account),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("credentials", hex256Array),
};

} // namespace rpc::spec::handlers::deposit_authorized
