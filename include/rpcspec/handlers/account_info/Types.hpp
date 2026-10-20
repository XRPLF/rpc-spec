/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <expected>

namespace rpc::spec::handlers::account_info {

/**
 * @brief Input for the 'account_info' RPC command.
 *
 * `queue` is read only on xrpld.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `account` request field, or of `ident` in its absence; on xrpld a
     * malformed one is reported by the handler. `Missing` when neither is sent, which only Clio
     * lets through.
     */
    DeferredAccountId account = std::unexpected{DeferredError::Missing};

    /**
     * @brief Value of the `signer_lists` request field.
     */
    JsonBool signerLists{false};

    /**
     * @brief Value of the `queue` request field.
     */
    JsonBool queue{false};
};

}  // namespace rpc::spec::handlers::account_info
