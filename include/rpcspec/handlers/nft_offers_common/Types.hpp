/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>

namespace rpc::spec::handlers::nft_offers_common {

inline constexpr uint32_t kLimitMin = 50;
inline constexpr uint32_t kLimitMax = 500;
inline constexpr uint32_t kLimitDefault = 250;

/**
 * @brief Input for the 'nft_buy_offers' / 'nft_sell_offers' RPC commands.
 */
struct Input
{
    LedgerSpecifier ledger;
    xrpl::uint256 nftID;
    uint32_t limit = kLimitDefault; /**< Clamped to [kLimitMin, kLimitMax] */
    std::optional<xrpl::uint256> marker;
};

}  // namespace rpc::spec::handlers::nft_offers_common
