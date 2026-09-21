/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::mptoken_issuance_history {

/**
 * @brief Smallest `limit` the handler accepts.
 */
inline constexpr auto kLimitMin = 1;

/**
 * @brief Largest `limit` the handler accepts; bigger values are clamped down.
 */
inline constexpr auto kLimitMax = 100;

/**
 * @brief `limit` applied when the request omits the field.
 */
inline constexpr auto kLimitDefault = 50;

/**
 * @brief Pagination marker for the 'mptoken_issuance_history' command.
 */
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
 * @brief Input for the 'mptoken_issuance_history' RPC command.
 *
 * @note Clio-only. When no ledger selector is given the request uses the backend's full
 * available ledger range, optionally narrowed by ledger_index_min/max.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `mpt_issuance_id` request field.
     */
    xrpl::uint192 mptIssuanceId;

    /**
     * @brief Value of the `account` request field.
     */
    std::optional<xrpl::AccountID> account;

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

    /**
     * @brief Validated tx-type name, kept as a normalized string.
     *
     * Same reason as account_tx: the valid set is derived at runtime from libxrpl TxFormats.
     */
    std::optional<std::string> transactionTypeInLowercase;
};

}  // namespace rpc::spec::handlers::mptoken_issuance_history
