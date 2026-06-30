/** @file */
#pragma once
// Shared constexpr spec for the 'account_mptokens' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/account_mptokens/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::account_mptokens {

inline constexpr auto kInputSpec = spec<Input>(
    field("account", &Input::account, required, accountIdActMalformed),
    field("ledger_hash", &Input::ledgerHash, ledgerHashHex),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32
    ),
    field("ledger_index", &Input::ledgerIndex, ledgerIndexOpt),
    field("marker", &Input::marker, accountMarker, asString),
    field("ledger", deprecated)
);

} // namespace rpc::spec::handlers::account_mptokens
