/** @file */
#pragma once
// Shared constexpr spec for the 'feature' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/feature/Types.hpp>

namespace rpc::spec::handlers::feature {

// `vetoed` is validate-only (always rejected via notSupported); no Input member.
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("feature", &Input::feature, asString),
    field(
        "vetoed",
        withCustomError(
            notSupported,
            RippledError::RpcNoPermission,
            "The admin portion of feature API is not available through Clio.")));

}  // namespace rpc::spec::handlers::feature
