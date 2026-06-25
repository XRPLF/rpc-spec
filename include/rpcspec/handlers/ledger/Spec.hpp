/** @file */
#pragma once
// Shared constexpr spec for the 'ledger' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// Fields with conditional validators:
//   ifServerClio(v)    — validator only runs in Clio builds
//   ifServerRippled(v) — validator only runs in rippled builds

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>

namespace rpc::spec::handlers::ledger {

inline constexpr auto kSpec = RpcSpec{
    field("ledger_hash") //
        | uint256Hex,
    field("ledger_index") //
        | ledgerIndex,
    field("transactions") //
        | type<bool>,
    field("expand") //
        | type<bool>,
    field("binary") //
        | type<bool>,
    field("owner_funds") //
        | type<bool>,

    field("queue")   //
        | type<bool> //
        | ifServerClio(notSupportedIf(true)),
    field("full")    //
        | type<bool> //
        | ifServerClio(notSupportedIf(true), deprecated),
    field("accounts") //
        | type<bool>  //
        | ifServerClio(notSupportedIf(true), deprecated),

    field("diff") //
        | ifServerClio(type<bool>),

    field("ledger") //
        | deprecated,
    field("type") //
        | deprecated,
};

} // namespace rpc::spec::handlers::ledger
