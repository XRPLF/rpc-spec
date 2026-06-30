/** @file */
#pragma once
// Shared constexpr spec for the 'book_changes' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Converters.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/book_changes/Types.hpp>

namespace rpc::spec::handlers::book_changes {

inline constexpr auto kInputSpec = spec<Input>(
    field("ledger_hash", &Input::ledgerHash, ledgerHashHex),
    field("ledger_index", &Input::ledgerIndex, ledgerIndexOpt)
);

} // namespace rpc::spec::handlers::book_changes
