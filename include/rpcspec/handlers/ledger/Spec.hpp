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

// All names (field, type, ifServerClio, ifServerRippled, notSupportedIf, notSupported, deprecated,
// uint256Hex, ledgerIndex) are found via unqualified lookup in the enclosing rpc::spec namespace.
inline constexpr auto kSpec = RpcSpec{
    field("ledger_hash",   uint256Hex),
    field("ledger_index",  ledgerIndex),
    field("transactions",  type<bool>),
    field("expand",        type<bool>),
    field("binary",        type<bool>),
    field("owner_funds",   type<bool>),

    // queue: fully supported in rippled; Clio rejects-if-true
    field("queue",         type<bool>, ifServerClio(notSupportedIf(true))),

    // full, accounts: supported in rippled; Clio treats as unsupported+deprecated
    field("full",          type<bool>, ifServerClio(notSupportedIf(true), deprecated)),
    field("accounts",      type<bool>, ifServerClio(notSupportedIf(true), deprecated)),

    // diff: Clio-specific extension; only type-checked in Clio; rippled ignores it entirely
    field("diff",          ifServerClio(type<bool>)),

    // deprecated in both servers
    field("ledger",        deprecated),
    field("type",          deprecated),
};

}  // namespace rpc::spec::handlers::ledger
