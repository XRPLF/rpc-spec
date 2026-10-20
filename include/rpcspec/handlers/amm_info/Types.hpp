/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/AssetTypes.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/Ledger.hpp>

#include <expected>

namespace rpc::spec::handlers::amm_info {

/**
 * @brief Input for the 'amm_info' RPC command.
 *
 * On xrpld every field but the ledger is deferred: the handler reports a malformed one after it
 * has resolved the ledger, in xrpld's order and with the ledger fields.
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
    DeferredAccountId accountID = std::unexpected{DeferredError::Missing};

    /**
     * @brief Value of the `amm_account` request field.
     */
    DeferredAccountId ammAccount = std::unexpected{DeferredError::Missing};

    /**
     * @brief Value of the `asset` request field.
     */
    DeferredAsset asset1 = std::unexpected{DeferredError::Missing};

    /**
     * @brief Value of the `asset2` request field.
     */
    DeferredAsset asset2 = std::unexpected{DeferredError::Missing};
};

}  // namespace rpc::spec::handlers::amm_info
