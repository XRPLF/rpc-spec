/** @file */
#pragma once
// Shared constexpr spec for the 'feature' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/feature/Types.hpp>

namespace rpc::spec::handlers::feature {

inline constexpr auto kSpec = RpcSpec{
    field("feature", type<std::string>),
    field(
        "vetoed",
        withCustomError(
            notSupported,
            RippledError::RpcNoPermission,
            "The admin portion of feature API is not available through Clio."
        )
    ),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
};

} // namespace rpc::spec::handlers::feature
