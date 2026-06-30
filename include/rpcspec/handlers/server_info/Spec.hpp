/** @file */
#pragma once
// Shared constexpr spec for the 'server_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/server_info/Types.hpp>

namespace rpc::spec::handlers::server_info {

inline constexpr auto kInputSpec = spec<Input>(
    field(kBackendCountersKey, &Input::backendCounters, jsonBool)
);

} // namespace rpc::spec::handlers::server_info
