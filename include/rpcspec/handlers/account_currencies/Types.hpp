/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

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
    xrpl::AccountID account;
    LedgerSpecifier ledger;
};

}  // namespace rpc::spec::handlers::account_currencies
