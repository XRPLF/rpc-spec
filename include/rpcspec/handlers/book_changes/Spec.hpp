/** @file */
#pragma once
// Shared constexpr spec for the 'book_changes' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/book_changes/Types.hpp>

namespace rpc::spec::handlers::book_changes {

inline constexpr auto kSpec = RpcSpec{
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
};

} // namespace rpc::spec::handlers::book_changes
