/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>

namespace rpc::spec::handlers::nft_offers_common {

/**
 * @brief Smallest `limit` the handler accepts.
 */
inline constexpr uint32_t kLimitMin = 50;

/**
 * @brief Largest `limit` the handler accepts; bigger values are clamped down.
 */
inline constexpr uint32_t kLimitMax = 500;

/**
 * @brief `limit` applied when the request omits the field.
 */
inline constexpr uint32_t kLimitDefault = 250;

/**
 * @brief Input for the 'nft_buy_offers' / 'nft_sell_offers' RPC commands.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `nft_id` request field.
     */
    xrpl::uint256 nftID;

    uint32_t limit;  ///< Clamped to [kLimitMin, kLimitMax]

    /**
     * @brief Value of the `marker` request field.
     */
    std::optional<xrpl::uint256> marker;
};

}  // namespace rpc::spec::handlers::nft_offers_common
