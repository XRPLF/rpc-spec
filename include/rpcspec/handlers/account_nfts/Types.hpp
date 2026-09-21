/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>

namespace rpc::spec::handlers::account_nfts {

/**
 * @brief Smallest `limit` the handler accepts.
 */
inline constexpr uint32_t kLimitMin = 20;

/**
 * @brief Largest `limit` the handler accepts; bigger values are clamped down.
 */
inline constexpr uint32_t kLimitMax = 400;

/**
 * @brief `limit` applied when the request omits the field.
 */
inline constexpr uint32_t kLimitDefault = 100;

/**
 * @brief Input for the 'account_nfts' RPC command.
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

    uint32_t limit;  ///< Clamped to [kLimitMin, kLimitMax]

    /**
     * @brief Value of the `marker` request field.
     */
    std::optional<xrpl::uint256> marker;
};

}  // namespace rpc::spec::handlers::account_nfts
