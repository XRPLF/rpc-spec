/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::book_offers {

inline constexpr uint32_t kLimitMin = 1;
inline constexpr uint32_t kLimitMax = 100;
inline constexpr uint32_t kLimitDefault = 60;

/**
 * @brief Input for the 'book_offers' RPC command.
 *
 * @note The taker is not really used in both Clio and `rippled`, both of them return all the
 * offers regardless of the funding status
 */
struct Input {
    LedgerSpecifier ledger;
    uint32_t limit = kLimitDefault;
    std::optional<xrpl::AccountID> taker;
    xrpl::Issue takerPays;
    xrpl::Issue takerGets;
    std::optional<std::string> domain;
};

} // namespace rpc::spec::handlers::book_offers
