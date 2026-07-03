/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>

namespace rpc::spec::handlers::transaction_entry {

/**
 * @brief Input for the 'transaction_entry' RPC command.
 */
struct Input
{
    LedgerSpecifier ledger;
    xrpl::uint256 txHash;
};

}  // namespace rpc::spec::handlers::transaction_entry
