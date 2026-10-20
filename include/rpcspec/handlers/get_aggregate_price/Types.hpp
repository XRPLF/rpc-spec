/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace rpc::spec::handlers::get_aggregate_price {

/**
 * @brief Largest `trim` the handler accepts, in percent.
 */
inline constexpr std::uint32_t kTrimMax = 25;

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
 * @brief The `oracles` of a request, or what is wrong with its first bad element.
 *
 * xrpld checks each element only after it has resolved the ledger, so on xrpld a bad one is
 * carried here: @ref DeferredError::Malformed for an element without both keys
 * (`oracleMalformed`), @ref DeferredError::Invalid for a bad document id or account
 * (`invalidParams`).
 */
using DeferredOracles = Deferred<std::vector<Oracle>>;

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
    DeferredOracles oracles = std::unexpected{DeferredError::Missing};

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
