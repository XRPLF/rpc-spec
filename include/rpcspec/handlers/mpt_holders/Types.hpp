/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rpc::spec::handlers::mpt_holders {

/**
 * @brief Smallest `limit` the handler accepts.
 */
inline constexpr uint32_t kLimitMin = 1;

/**
 * @brief Largest `limit` the handler accepts; bigger values are clamped down.
 */
inline constexpr uint32_t kLimitMax = 100;

/**
 * @brief `limit` applied when the request omits the field.
 */
inline constexpr uint32_t kLimitDefault = 50;

/**
 * @brief Largest number of accounts the `accounts` filter accepts.
 */
inline constexpr std::size_t kMaxAccounts = 100;

/**
 * @brief Input for the 'mpt_holders' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `mpt_issuance_id` request field.
     */
    xrpl::uint192 mptID;

    /**
     * @brief Value of the `marker` request field.
     */
    std::optional<xrpl::AccountID> marker;

    /**
     * @brief The client-supplied page size, unset when the request omits it.
     *
     * @note Left optional rather than defaulted so the handler can distinguish an explicit
     * limit from an absent one; it applies `kLimitDefault` when unset.
     */
    std::optional<uint32_t> limit;

    /**
     * @brief Look up only these accounts, instead of walking the holder index.
     *
     * @note Clio-only: xrpld does not serve `mpt_holders`. The filtered lookup is bounded and
     * unpaginated, so it cannot be combined with `marker` or `limit`.
     */
    std::optional<std::vector<xrpl::AccountID>> accounts;
};

}  // namespace rpc::spec::handlers::mpt_holders
