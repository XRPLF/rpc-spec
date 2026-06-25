/** @file */
#pragma once
// Shared constexpr spec for the 'account_objects' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/account_objects/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_objects {

inline constexpr auto kSpec = RpcSpec{
    field("account", required, account),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field("type", accountType),
    field("marker", accountMarker),
    field("deletion_blockers_only", type<bool>),
};

} // namespace rpc::spec::handlers::account_objects
