/** @file */
#pragma once
// Shared constexpr spec for the 'account_channels' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/account_channels/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_channels {

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field("destination_account", &Input::destinationAccount, type<std::string>, accountId),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32),
    field("marker", &Input::marker, accountMarker, asString));

}  // namespace rpc::spec::handlers::account_channels
