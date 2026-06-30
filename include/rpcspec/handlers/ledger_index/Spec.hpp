/** @file */
#pragma once
// Shared constexpr spec for the 'ledger_index' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/ledger_index/Types.hpp>

namespace rpc::spec::handlers::ledger_index {

inline constexpr auto kInputSpec = spec<Input>(
    field("date", &Input::date, timeFormat(kDateFormat), asString)
);

} // namespace rpc::spec::handlers::ledger_index
