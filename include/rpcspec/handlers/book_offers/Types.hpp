/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Asset.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::book_offers {

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
inline constexpr uint32_t kLimitDefault = 60;

/**
 * @brief Input for the 'book_offers' RPC command.
 *
 * @note The taker is not really used in both Clio and `xrpld`, both of them return all the
 * offers regardless of the funding status
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `limit` request field.
     */
    uint32_t limit;

    /**
     * @brief Value of the `taker` request field.
     */
    std::optional<xrpl::AccountID> taker;

    /**
     * @brief Value of the `taker_pays` request field.
     */
    xrpl::Asset takerPays = xrpl::xrpIssue();

    /**
     * @brief Value of the `taker_gets` request field.
     */
    xrpl::Asset takerGets = xrpl::xrpIssue();
    std::optional<std::string>
        domain;  ///< Permissioned-domain id, passed through as a validated hex string.
};

}  // namespace rpc::spec::handlers::book_offers
