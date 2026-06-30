/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::amm_info {

/**
 * @brief Input for the 'amm_info' RPC command.
 */
struct Input {
    std::optional<xrpl::AccountID> accountID;
    std::optional<xrpl::AccountID> ammAccount;
    xrpl::Issue issue1 = xrpl::noIssue();
    xrpl::Issue issue2 = xrpl::noIssue();
    LedgerSpecifier ledger;
};

} // namespace rpc::spec::handlers::amm_info
