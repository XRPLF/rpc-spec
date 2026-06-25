/** @file */
#pragma once
// Shared constexpr spec for the 'account_nfts' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/account_nfts/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_nfts {

inline constexpr auto kSpec = RpcSpec{
    field("account", required, account),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("marker", uint256Hex),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
};

} // namespace rpc::spec::handlers::account_nfts
