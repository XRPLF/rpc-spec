/** @file */
#pragma once
// Shared constexpr spec for the 'nft_history' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/nft_history/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::nft_history {

inline constexpr auto kSpec = RpcSpec{
    field("nft_id", required, uint256Hex),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("ledger_index_min", type<int64_t>),
    field("ledger_index_max", type<int64_t>),
    field("binary", type<bool>),
    field("forward", type<bool>),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field(
        "marker",
        withCustomError(
            type<JsonObject>, rpc::RippledError::RpcInvalidParams, "invalidMarker"
        ),
        ifType<JsonObject>(section(
            field("ledger", required, type<uint32_t>),
            field("seq", required, type<uint32_t>)
        ))
    ),
};

} // namespace rpc::spec::handlers::nft_history
