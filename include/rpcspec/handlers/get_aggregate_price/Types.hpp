/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rpc::spec::handlers::get_aggregate_price {

/**
 * @brief A struct to hold the input oracle data
 */
struct Oracle {
    std::uint32_t documentId{0};
    xrpl::AccountID account;
};

/**
 * @brief Input for the 'get_aggregate_price' RPC command.
 */
struct Input {
    std::optional<std::string> ledgerHash;
    std::optional<std::uint32_t> ledgerIndex;
    std::vector<Oracle> oracles;  // valid range is 1-200
    std::string baseAsset;
    std::string quoteAsset;
    std::optional<std::uint32_t> timeThreshold;
    std::optional<std::uint8_t> trim;  // valid range is 1-25
};

} // namespace rpc::spec::handlers::get_aggregate_price
