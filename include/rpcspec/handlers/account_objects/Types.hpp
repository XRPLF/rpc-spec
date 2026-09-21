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
 * @brief Input for the 'account_objects' RPC command.
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

    /**
     * @brief Value of the `type` request field.
     */
    std::optional<xrpl::LedgerEntryType> type;

    /**
     * @brief Value of the `deletion_blockers_only` request field.
     */
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
