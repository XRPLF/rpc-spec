/** @file */
#pragma once
// Shared constexpr spec for the 'account_info' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// V1: account, ident (deprecated), ledger_hash, ledger_index, ledger
//     (deprecated), strict (deprecated)
// V2: V1 + signer_lists

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/account_info/Types.hpp>

namespace rpc::spec::handlers::account_info {

inline constexpr auto kSpecV1 = RpcSpec{
    field("account", account),
    field("ident", account, deprecated),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("ledger", deprecated),
    field("strict", deprecated),
};

inline constexpr auto kSpecV2 = extend(kSpecV1, field("signer_lists", type<bool>));

}  // namespace rpc::spec::handlers::account_info
