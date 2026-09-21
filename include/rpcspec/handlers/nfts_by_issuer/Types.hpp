/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nfts_by_issuer {

/**
 * @brief Smallest `limit` the handler accepts.
 */
inline constexpr uint32_t kLimitMin = 1;

/**
 * @brief Largest `limit` the handler accepts; bigger values are clamped down.
 */
inline constexpr uint32_t kLimitMax = 100;

/**
 * @brief `limit` applied when the request omits the field.
 */
inline constexpr uint32_t kLimitDefault = 50;

/**
 * @brief Input for the 'nfts_by_issuer' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `issuer` request field.
     */
    xrpl::AccountID issuer;

    /**
     * @brief Value of the `nft_taxon` request field.
     */
    std::optional<uint32_t> nftTaxon;

    /**
     * @brief Value of the `marker` request field.
     */
    std::optional<xrpl::uint256> marker;
    uint32_t limit;  ///< Clamped to [kLimitMin, kLimitMax]
};

}  // namespace rpc::spec::handlers::nfts_by_issuer
