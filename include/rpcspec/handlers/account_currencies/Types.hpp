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
 * @brief Largest `limit` the handler accepts; bigger values are clamped down. xrpld clamps with
 * its own limits, which match these, and lets an admin go past this.
 */
inline constexpr uint32_t kLimitMax = 10'000;

/**
 * @brief `limit` applied when the request omits the field: as many owner directory entries as
 * `kLimitMax`, so an account with fewer gets every currency in one response.
 */
inline constexpr uint32_t kLimitDefault = 10'000;

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
