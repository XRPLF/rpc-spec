/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_lines {

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
 * @brief Input for the 'account_lines' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `account` request field; on xrpld a malformed one is reported by the
     * handler.
     */
    DeferredAccountId account;

    /**
     * @brief Value of the `peer` request field; on xrpld a malformed one is reported by the
     * handler, and an empty one reads as absent.
     */
    DeferredAccountId peer = std::unexpected{AccountError::Missing};

    /**
     * @brief Value of the `ignore_default` request field.
     */
    bool ignoreDefault = false;  // TODO: document
                                 // https://github.com/XRPLF/xrpl-dev-portal/issues/1839
    /**
     * @brief Value of the `limit` request field.
     */
    uint32_t limit;

    /**
     * @brief Pagination cursor, `"<uint256hex>,<uint64>"`, kept as the validated string.
     */
    std::optional<std::string> marker;
};

}  // namespace rpc::spec::handlers::account_lines
