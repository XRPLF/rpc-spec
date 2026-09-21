/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <rpcspec/JsonBool.hpp>
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

    /**
     * @brief Opaque pagination cursor; may encode an account plus a hint, not a single id.
     *
     * Re-parsed by traverseOwnedNodes downstream, so kept as a validated string.
     */
    std::optional<std::string> marker;
    std::optional<xrpl::LedgerEntryType> type;
    bool deletionBlockersOnly = false;

    /**
     * @brief Tri-state sponsorship filter.
     *
     * Unset means no sponsorship filter; set restricts results to objects that are (true)
     * or are not (false) sponsored.
     */
    std::optional<JsonBool> sponsored;
};

}  // namespace rpc::spec::handlers::account_objects
