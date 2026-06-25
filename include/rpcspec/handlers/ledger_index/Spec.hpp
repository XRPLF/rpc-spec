/** @file */
#pragma once
// Shared constexpr spec for the 'ledger_index' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/ledger_index/Types.hpp>

namespace rpc::spec::handlers::ledger_index {

inline constexpr auto kSpec = RpcSpec{
    field(
        "date",
        type<std::string>,
        timeFormat(kDateFormat)
    ),
};

} // namespace rpc::spec::handlers::ledger_index
