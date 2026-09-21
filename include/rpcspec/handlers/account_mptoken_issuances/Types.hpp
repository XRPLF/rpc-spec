/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_mptoken_issuances {

inline constexpr uint32_t kLimitMin = 10;
inline constexpr uint32_t kLimitMax = 400;
inline constexpr uint32_t kLimitDefault = 200;

/**
 * @brief Input for the 'account_mptoken_issuances' RPC command.
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
};

}  // namespace rpc::spec::handlers::account_mptoken_issuances
