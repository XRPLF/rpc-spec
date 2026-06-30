/** @file */
#pragma once
// Shared constexpr spec for the 'account_info' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// V1: account, ident (deprecated), ledger_hash, ledger_index, ledger
//     (deprecated), strict (deprecated)
// V2: V1 + signer_lists

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/account_info/Types.hpp>

namespace rpc::spec::handlers::account_info {

inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, accountId),
    field("ident", &Input::ident) | deprecated | accountId,
    field("signer_lists", &Input::signerLists, jsonBool),
    field("ledger", deprecated),
    field("strict", deprecated)
);

inline constexpr auto kInputSpecV2 = extend(kInputSpecV1, field("signer_lists", &Input::signerLists, jsonBoolStrict));

}  // namespace rpc::spec::handlers::account_info
