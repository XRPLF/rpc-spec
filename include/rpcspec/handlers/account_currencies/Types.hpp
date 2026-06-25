/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_currencies {

/**
 * @brief Input for the 'account_currencies' RPC command.
 */
struct Input {
  std::string account;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
};

} // namespace rpc::spec::handlers::account_currencies
