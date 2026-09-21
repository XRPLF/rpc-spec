/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nft_history {

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
 * @brief A struct to hold the marker data
 */
// TODO: this marker is same as account_tx, reuse in future
struct Marker
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    uint32_t ledger;

    /**
     * @brief Value of the `seq` field.
     */
    uint32_t seq;
};

/**
 * @brief Input for the 'nft_history' RPC command.
 */
struct Input
{
    // You must use at least one of the following fields in your request:
    // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
    // `ledger` is unspecified when none of ledger_hash/ledger_index is given, so
    // the handler can choose between range mode (min/max) and the default ledger.
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `nft_id` request field.
     */
    xrpl::uint256 nftID;

    /**
     * @brief Value of the `ledger_index_min` request field.
     */
    std::optional<int32_t> ledgerIndexMin;

    /**
     * @brief Value of the `ledger_index_max` request field.
     */
    std::optional<int32_t> ledgerIndexMax;

    /**
     * @brief Value of the `binary` request field.
     */
    JsonBool binary{false};

    /**
     * @brief Value of the `forward` request field.
     */
    JsonBool forward{false};

    /**
     * @brief Value of the `limit` request field.
     */
    std::optional<uint32_t> limit;

    /**
     * @brief Value of the `marker` request field.
     */
    std::optional<Marker> marker;
};

}  // namespace rpc::spec::handlers::nft_history
