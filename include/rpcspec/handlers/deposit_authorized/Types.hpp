/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace rpc::spec::handlers::deposit_authorized {

/**
 * @brief Input for the 'deposit_authorized' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `source_account` request field.
     */
    xrpl::AccountID sourceAccount;

    /**
     * @brief Value of the `destination_account` request field.
     */
    xrpl::AccountID destinationAccount;

    /**
     * @brief Value of the `credentials` request field.
     */
    std::optional<std::vector<xrpl::uint256>> credentials;
};

}  // namespace rpc::spec::handlers::deposit_authorized
