/** @file */
#pragma once
// Shared constexpr spec for the 'account_offers' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_offers {

inline constexpr auto kSpec = RpcSpec{
    field("account", required, account),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("marker", accountMarker),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field("ledger", deprecated),
    field("strict", deprecated),
};

} // namespace rpc::spec::handlers::account_offers
