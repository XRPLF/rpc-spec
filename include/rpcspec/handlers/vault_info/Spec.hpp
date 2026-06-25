/** @file */
#pragma once
// Shared constexpr spec for the 'vault_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/vault_info/Types.hpp>

namespace rpc::spec::handlers::vault_info {

inline constexpr auto kSpec = RpcSpec{
    field("vault_id", withCustomError(uint256Hex, rpc::ClioError::RpcMalformedRequest)),
    field(
        "owner",
        withCustomError(accountBase58, rpc::ClioError::RpcMalformedRequest, "OwnerNotHexString")
    ),
    field("seq", withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)),
    field("ledger_index", ledgerIndex),
};

} // namespace rpc::spec::handlers::vault_info
