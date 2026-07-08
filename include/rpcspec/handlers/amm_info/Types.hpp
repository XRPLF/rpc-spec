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
    LedgerSpecifier ledger;
    std::optional<xrpl::AccountID> accountID;
    std::optional<xrpl::AccountID> ammAccount;
    xrpl::Issue issue1 = xrpl::noIssue();
    xrpl::Issue issue2 = xrpl::noIssue();
};

}  // namespace rpc::spec::handlers::amm_info
