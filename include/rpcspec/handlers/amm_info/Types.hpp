/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::amm_info {

/**
 * @brief Input for the 'amm_info' RPC command.
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
    std::optional<xrpl::AccountID> accountID;

    /**
     * @brief Value of the `amm_account` request field.
     */
    std::optional<xrpl::AccountID> ammAccount;

    /**
     * @brief Value of the `asset` request field.
     */
    xrpl::Issue issue1 = xrpl::noIssue();

    /**
     * @brief Value of the `asset2` request field.
     */
    xrpl::Issue issue2 = xrpl::noIssue();
};

}  // namespace rpc::spec::handlers::amm_info
