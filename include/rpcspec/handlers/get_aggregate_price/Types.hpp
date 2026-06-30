/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace rpc::spec::handlers::get_aggregate_price {

/**
 * @brief A struct to hold the input oracle data
 */
struct Oracle
{
    std::uint32_t documentId{0};
    xrpl::AccountID account;
};

/**
 * @brief Input for the 'get_aggregate_price' RPC command.
 */
struct Input
{
    LedgerSpecifier ledger;
    std::vector<Oracle> oracles;
    xrpl::Currency baseAsset;
    xrpl::Currency quoteAsset;
    std::optional<std::uint32_t> timeThreshold;
    std::optional<std::uint8_t> trim;
};

}  // namespace rpc::spec::handlers::get_aggregate_price
