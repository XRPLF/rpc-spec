/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>

namespace rpc::spec::handlers::vault_info {

/**
 * @brief Input for the 'vault_info' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `vault_id` request field.
     */
    std::optional<xrpl::uint256> vaultID;

    /**
     * @brief Value of the `owner` request field.
     */
    std::optional<xrpl::AccountID> owner;

    /**
     * @brief Value of the `seq` request field.
     */
    std::optional<uint32_t> tnxSequence;
};

}  // namespace rpc::spec::handlers::vault_info
