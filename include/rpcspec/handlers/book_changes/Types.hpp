/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::book_changes {

/**
 * @brief Input for the 'book_changes' RPC command.
 *
 * @note Clio does not implement `deletion_blockers_only`
 */
struct Input {
  LedgerSpecifier ledger;
};

} // namespace rpc::spec::handlers::book_changes
