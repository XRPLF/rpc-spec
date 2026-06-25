/** @file */
#pragma once
// Shared constexpr spec for the 'nfts_by_issuer' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/nfts_by_issuer/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::nfts_by_issuer {

inline constexpr auto kSpec = RpcSpec{
    field("issuer", required, account),
    field("nft_taxon", type<uint32_t>),
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

} // namespace rpc::spec::handlers::nfts_by_issuer
