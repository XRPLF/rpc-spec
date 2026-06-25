/** @file */
#pragma once
// Shared constexpr spec for the 'mpt_holders' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::mpt_holders {

inline constexpr auto kSpec = RpcSpec{
    field("mpt_issuance_id", required, uint192Hex),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field("marker", uint160Hex),
};

} // namespace rpc::spec::handlers::mpt_holders
