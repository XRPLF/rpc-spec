/** @file */
#pragma once
// Shared constexpr spec for the 'account_channels' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/account_channels/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_channels {

inline constexpr auto kSpec = RpcSpec{
    field("account", required, account),
    // Type<std::string> is chained before `account` so that a non-string
    // destination_account produces a bare RpcInvalidParams rather than the
    // "<key>NotString" message that AccountFormat would emit.
    field("destination_account", type<std::string>, account),
    field("ledger_hash", uint256Hex),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field("ledger_index", ledgerIndex),
    field("marker", accountMarker),
};

} // namespace rpc::spec::handlers::account_channels
