/** @file */
#pragma once

#include <optional>
#include <string>
#include <vector>

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>

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
    Server,       ///< rippled only (admin-gated downstream); not served by Clio.
    PeerStatus,   ///< rippled only (admin-gated downstream); not served by Clio.
    Consensus,    ///< rippled only; not served by Clio.
};

/**
 * @brief A struct to hold one order book
 */
struct OrderBook {
    xrpl::Book book;
    bool both = false;
};

/**
 * @brief Input for the 'unsubscribe' RPC command.
 */
struct Input {
    std::optional<std::vector<xrpl::AccountID>> accounts;
    std::optional<std::vector<StreamType>> streams;
    std::optional<std::vector<xrpl::AccountID>> accountsProposed;
    std::optional<std::vector<OrderBook>> books;
};

} // namespace rpc::spec::handlers::unsubscribe
