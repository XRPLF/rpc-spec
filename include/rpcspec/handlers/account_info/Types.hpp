/** @file */
#pragma once

#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <optional>

namespace rpc::spec::handlers::account_info {

/**
 * @brief Input for the 'account_info' RPC command.
 *
 * `ident` is the fallback for a missing `account`, and is deprecated on Clio. `queue` is read
 * only on xrpld.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `account` request field; on xrpld a malformed one is reported by the
     * handler.
     */
    std::optional<DeferredAccountId> account;

    /**
     * @brief Value of the `ident` request field; on xrpld set only when `account` is absent.
     */
    std::optional<DeferredAccountId> ident;

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
