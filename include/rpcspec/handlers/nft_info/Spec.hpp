/** @file */
#pragma once
// Shared constexpr spec for the 'nft_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/nft_info/Types.hpp>

namespace rpc::spec::handlers::nft_info {

inline constexpr auto kSpec = RpcSpec{
    field("nft_id", required, uint256Hex),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
};

} // namespace rpc::spec::handlers::nft_info
