/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::transaction_entry {

/**
 * @brief Input for the 'transaction_entry' RPC command.
 */
struct Input {
  std::string txHash;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
};

} // namespace rpc::spec::handlers::transaction_entry
