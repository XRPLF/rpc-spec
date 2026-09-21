/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_mptoken_issuances {

/**
 * @brief Smallest `limit` the handler accepts.
 */
inline constexpr uint32_t kLimitMin = 10;

/**
 * @brief Largest `limit` the handler accepts; bigger values are clamped down.
 */
inline constexpr uint32_t kLimitMax = 400;

/**
 * @brief `limit` applied when the request omits the field.
 */
inline constexpr uint32_t kLimitDefault = 200;

/**
 * @brief Input for the 'account_mptoken_issuances' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `account` request field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `limit` request field.
     */
    uint32_t limit;

    /**
     * @brief Opaque pagination cursor; may encode an account plus a hint, not a single id.
     *
     * Re-parsed by traverseOwnedNodes downstream, so kept as a validated string.
     */
    std::optional<std::string> marker;
};

}  // namespace rpc::spec::handlers::account_mptoken_issuances
