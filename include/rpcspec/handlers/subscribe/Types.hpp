/** @file */
#pragma once

#include <xrpl/protocol/Book.h>

#include <optional>
#include <string>
#include <vector>

namespace rpc::spec::handlers::subscribe {

/**
 * @brief A struct to hold the data for one order book
 */
struct OrderBook {
    xrpl::Book book;
    std::optional<std::string> taker;
    bool snapshot = false;
    bool both = false;
};

/**
 * @brief Input for the 'subscribe' RPC command.
 */
struct Input {
    std::optional<std::vector<std::string>> accounts;
    std::optional<std::vector<std::string>> streams;
    std::optional<std::vector<std::string>> accountsProposed;
    std::optional<std::vector<OrderBook>> books;
};

} // namespace rpc::spec::handlers::subscribe
