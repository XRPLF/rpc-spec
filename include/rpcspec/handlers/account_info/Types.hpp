/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_info {

/**
 * @brief Input for the 'account_info' RPC command.
 *
 * `queue` is not available in Reporting mode
 * `ident` is deprecated, keep it for now, in line with xrpld
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
    std::optional<xrpl::AccountID> account;

    /**
     * @brief Value of the `ident` request field.
     */
    std::optional<xrpl::AccountID> ident;

    /**
     * @brief Value of the `signer_lists` request field.
     */
    JsonBool signerLists{false};
};

}  // namespace rpc::spec::handlers::account_info
