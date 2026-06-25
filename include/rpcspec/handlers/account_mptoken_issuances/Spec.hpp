/** @file */
#pragma once
// Shared constexpr spec for the 'account_mptoken_issuances' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/account_mptoken_issuances/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_mptoken_issuances {

inline constexpr auto kSpec = RpcSpec{
    field("account", required, withCustomError(account, RippledError::RpcActMalformed)),
    field("ledger_hash", uint256Hex),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field("ledger_index", ledgerIndex),
    field("marker", accountMarker),
    field("ledger", deprecated),
};

} // namespace rpc::spec::handlers::account_mptoken_issuances
