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

inline constexpr uint32_t kLimitMin = 1;
inline constexpr uint32_t kLimitMax = 100;
inline constexpr uint32_t kLimitDefault = 50;

/** @brief Largest number of accounts the `accounts` filter accepts. */
inline constexpr std::size_t kMaxAccounts = 100;

/**
 * @brief Input for the 'mpt_holders' RPC command.
 */
struct Input
{
    LedgerSpecifier ledger;
    xrpl::uint192 mptID;
    std::optional<xrpl::AccountID> marker;
    /**
     * @brief The client-supplied page size, unset when the request omits it.
     *
     * @note Left optional rather than defaulted so the handler can distinguish an explicit
     * limit from an absent one; it applies @ref kLimitDefault when unset.
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
