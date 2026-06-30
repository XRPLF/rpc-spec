/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_objects {

inline constexpr uint32_t kLimitMin = 10;
inline constexpr uint32_t kLimitMax = 400;
inline constexpr uint32_t kLimitDefault = 200;

/**
 * @brief Input for the 'account_objects' RPC command.
 */
struct Input {
    xrpl::AccountID account;
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    uint32_t limit = kLimitDefault;  // [kLimitMin, kLimitMax]
    std::optional<std::string> marker;
    std::optional<xrpl::LedgerEntryType> type;
    bool deletionBlockersOnly = false;
};

}  // namespace rpc::spec::handlers::account_objects
