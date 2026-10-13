/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_channels {

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
 * @brief Input for the 'account_channels' RPC command.
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
     * @brief Value of the `destination_account` request field.
     */
    std::optional<xrpl::AccountID> destinationAccount;

    /**
     * @brief Value of the `limit` request field.
     */
    uint32_t limit;

    /**
     * @brief Pagination cursor, `"<uint256hex>,<uint64>"`, kept as the validated string.
     */
    std::optional<std::string> marker;
};

}  // namespace rpc::spec::handlers::account_channels
