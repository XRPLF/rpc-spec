/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_nfts {

inline constexpr uint32_t kLimitMin = 20;
inline constexpr uint32_t kLimitMax = 400;
inline constexpr uint32_t kLimitDefault = 100;

/**
 * @brief Input for the 'account_nfts' RPC command.
 */
struct Input {
  std::string account;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  uint32_t limit = kLimitDefault;  // Limit the number of token pages to retrieve. [20,400]
  std::optional<std::string> marker;
};

} // namespace rpc::spec::handlers::account_nfts
