/** @file */
#pragma once
// Shared constexpr spec for the 'ledger_data' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/ledger_data/Types.hpp>

namespace rpc::spec::handlers::ledger_data {

inline constexpr auto kSpec = RpcSpec{
    field("binary", type<bool>),
    field("out_of_order", type<bool>),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("limit", type<uint32_t>, min(uint32_t{1})),
    field("marker", type<uint32_t, std::string>, ifType<std::string>(uint256Hex)),
    field("type", ledgerType),
    field("ledger", deprecated),
};

} // namespace rpc::spec::handlers::ledger_data
