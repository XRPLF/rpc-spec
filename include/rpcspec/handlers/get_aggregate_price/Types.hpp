/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace rpc::spec::handlers::get_aggregate_price {

/**
 * @brief A struct to hold the input oracle data
 */
struct Oracle
{
    /**
     * @brief Value of the `document_id` field.
     */
    std::uint32_t documentId{0};

    /**
     * @brief Value of the `account` field.
     */
    xrpl::AccountID account;
};

/**
 * @brief Input for the 'get_aggregate_price' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `oracles` request field.
     */
    std::vector<Oracle> oracles;

    /**
     * @brief Value of the `base_asset` request field.
     */
    xrpl::Currency baseAsset;

    /**
     * @brief Value of the `quote_asset` request field.
     */
    xrpl::Currency quoteAsset;

    /**
     * @brief Value of the `time_threshold` request field.
     */
    std::optional<std::uint32_t> timeThreshold;

    /**
     * @brief Value of the `trim` request field.
     */
    std::optional<std::uint8_t> trim;
};

}  // namespace rpc::spec::handlers::get_aggregate_price
