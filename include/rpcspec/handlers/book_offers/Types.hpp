/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
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
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    uint32_t limit = kLimitDefault;
    std::optional<xrpl::AccountID> taker;
    xrpl::Currency paysCurrency;
    xrpl::Currency getsCurrency;
    // accountID will be filled by input converter, if no issuer is given, will use XRP issuer
    xrpl::AccountID paysID = xrpl::xrpAccount();
    xrpl::AccountID getsID = xrpl::xrpAccount();
    std::optional<std::string> domain;
};

} // namespace rpc::spec::handlers::book_offers
