/** @file */
#pragma once

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
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
};

} // namespace rpc::spec::handlers::book_changes
