/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>

namespace rpc::spec::handlers::vault_info {

/**
 * @brief Input for the 'vault_info' RPC command.
 */
struct Input {
    std::optional<xrpl::uint256> vaultID;
    std::optional<xrpl::AccountID> owner;
    std::optional<uint32_t> tnxSequence;
    LedgerSpecifier ledger;
};

} // namespace rpc::spec::handlers::vault_info
