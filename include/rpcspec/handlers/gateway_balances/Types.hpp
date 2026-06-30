/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace rpc::spec::handlers::gateway_balances {

/**
 * @brief Input for the 'gateway_balances' RPC command.
 */
struct Input {
    xrpl::AccountID account;
    std::set<xrpl::AccountID> hotWallets;
    LedgerSpecifier ledger;
};

} // namespace rpc::spec::handlers::gateway_balances
