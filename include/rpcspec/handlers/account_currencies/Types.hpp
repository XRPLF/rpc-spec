/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_currencies {

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
 * @brief Input for the 'account_currencies' RPC command.
 *
 * `limit` and `marker` page through the owner directory, and are read only on xrpld.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `account` request field (or, on xrpld, of `ident` in its absence).
     */
    DeferredAccountId account;

    /**
     * @brief Value of the `limit` request field: owner directory entries scanned per page.
     */
    uint32_t limit;

    /**
     * @brief Pagination cursor, `"<uint256hex>,<uint64>"`, kept as the validated string.
     */
    std::optional<std::string> marker;
};

}  // namespace rpc::spec::handlers::account_currencies
