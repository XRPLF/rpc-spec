/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_currencies {

/**
 * @brief Input for the 'account_currencies' RPC command.
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
};

}  // namespace rpc::spec::handlers::account_currencies
