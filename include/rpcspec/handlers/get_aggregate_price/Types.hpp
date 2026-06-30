/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
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
    LedgerSpecifier ledger;
    std::vector<Oracle> oracles;  // valid range is 1-200
    xrpl::Currency baseAsset;
    xrpl::Currency quoteAsset;
    std::optional<std::uint32_t> timeThreshold;
    std::optional<std::uint8_t> trim;  // valid range is 1-25
};

} // namespace rpc::spec::handlers::get_aggregate_price
