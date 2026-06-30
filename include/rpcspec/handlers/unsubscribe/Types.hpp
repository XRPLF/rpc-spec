/** @file */
#pragma once

#include <optional>
#include <string>
#include <vector>

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>

namespace rpc::spec::handlers::unsubscribe {

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
    std::optional<std::vector<std::string>> streams;
    std::optional<std::vector<xrpl::AccountID>> accountsProposed;
    std::optional<std::vector<OrderBook>> books;
};

} // namespace rpc::spec::handlers::unsubscribe
