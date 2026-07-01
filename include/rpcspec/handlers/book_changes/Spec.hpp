/** @file */
#pragma once
// Shared constexpr spec for the 'book_changes' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/book_changes/Types.hpp>

namespace rpc::spec::handlers::book_changes {

inline constexpr auto kInputSpec = spec<Input>(ledgerSelector(&Input::ledger));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::book_changes
