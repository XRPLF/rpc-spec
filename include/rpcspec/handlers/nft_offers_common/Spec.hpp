/** @file */
#pragma once
// Shared constexpr spec for the 'nft_buy_offers' / 'nft_sell_offers' RPC commands.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/nft_offers_common/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::nft_offers_common {

inline constexpr auto kSpec = RpcSpec{
    field("nft_id", required, uint256Hex),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field("marker", uint256Hex),
};

} // namespace rpc::spec::handlers::nft_offers_common
