/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>

#include <optional>
#include <string>
#include <vector>

namespace rpc::spec::handlers::unsubscribe {

/**
 * @brief A subscribable stream. Mirrors the wire stream names accepted by the
 * spec; unsupported/unknown names are rejected during validation.
 */
enum class StreamType {
    Ledger,
    Transactions,
    TransactionsProposed,  ///< Also the target of the deprecated `rt_transactions` alias.
    BookChanges,
    Manifests,
    Validations,
    Server,      ///< xrpld only (admin-gated downstream); not served by Clio.
    PeerStatus,  ///< xrpld only (admin-gated downstream); not served by Clio.
    Consensus,   ///< xrpld only; not served by Clio.
};

/**
 * @brief A struct to hold one order book
 */
struct OrderBook
{
    /**
     * @brief Value of the `book` field.
     */
    xrpl::Book book;

    /**
     * @brief Value of the `both` field.
     */
    bool both = false;
};

/**
 * @brief Input for the 'unsubscribe' RPC command.
 */
struct Input
{
    /**
     * @brief Value of the `accounts` request field.
     */
    std::optional<std::vector<xrpl::AccountID>> accounts;

    /**
     * @brief Value of the `streams` request field.
     */
    std::optional<std::vector<StreamType>> streams;

    /**
     * @brief Value of the `accounts_proposed` request field.
     */
    std::optional<std::vector<xrpl::AccountID>> accountsProposed;

    /**
     * @brief Value of the `books` request field.
     */
    std::optional<std::vector<OrderBook>> books;
};

}  // namespace rpc::spec::handlers::unsubscribe
