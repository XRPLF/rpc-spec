/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace rpc::spec::handlers::gateway_balances {

/**
 * @brief Input for the 'gateway_balances' RPC command.
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
     * @brief Value of the `hotwallet` request field.
     */
    std::set<xrpl::AccountID> hotWallets;
};

}  // namespace rpc::spec::handlers::gateway_balances
