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
 * `ident` is deprecated, keep it for now, in line with rippled
 */
struct Input
{
    std::optional<xrpl::AccountID> account;
    std::optional<xrpl::AccountID> ident;
    LedgerSpecifier ledger;
    JsonBool signerLists{false};
};

}  // namespace rpc::spec::handlers::account_info
