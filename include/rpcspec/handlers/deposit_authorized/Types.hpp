/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace rpc::spec::handlers::deposit_authorized {

/**
 * @brief Input for the 'deposit_authorized' RPC command.
 */
struct Input
{
    LedgerSpecifier ledger;
    xrpl::AccountID sourceAccount;
    xrpl::AccountID destinationAccount;
    std::optional<std::vector<xrpl::uint256>> credentials;
};

}  // namespace rpc::spec::handlers::deposit_authorized
