/** @file */
#pragma once
// Shared constexpr spec for the 'server_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/server_info/Types.hpp>

namespace rpc::spec::handlers::server_info {

// server_info accepts no validated fields — all parsing is done in tag_invoke.
inline constexpr auto kSpec = RpcSpec{};

} // namespace rpc::spec::handlers::server_info
