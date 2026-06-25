/** @file */
#pragma once
// Shared constexpr spec for the 'tx' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// V1: transaction, min_ledger, max_ledger, ctid
// V2: V1 + binary

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/tx/Types.hpp>

namespace rpc::spec::handlers::tx {

inline constexpr auto kSpecV1 = RpcSpec{
    field("transaction", uint256Hex),
    field("min_ledger", type<uint32_t>),
    field("max_ledger", type<uint32_t>),
    field("ctid", type<std::string>),
};

inline constexpr auto kSpecV2 = extend(kSpecV1, field("binary", type<bool>));

} // namespace rpc::spec::handlers::tx
