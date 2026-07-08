/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <rpcspec/Ledger.hpp>

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
struct Input
{
    LedgerSpecifier ledger;
    xrpl::AccountID account;
    uint32_t limit;
    std::optional<std::string>
        marker; /**< Opaque pagination cursor (may encode an account + hint, not a single id);
                   re-parsed by traverseOwnedNodes downstream, so kept as a validated string. */
    std::optional<xrpl::LedgerEntryType> type;
    bool deletionBlockersOnly = false;
};

}  // namespace rpc::spec::handlers::account_objects
