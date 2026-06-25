/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_lines {

inline constexpr uint32_t kLimitMin = 10;
inline constexpr uint32_t kLimitMax = 400;
inline constexpr uint32_t kLimitDefault = 200;

/**
 * @brief Input for the 'account_lines' RPC command.
 */
struct Input {
  std::string account;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  std::optional<std::string> peer;
  bool ignoreDefault = false;  // TODO: document
                               // https://github.com/XRPLF/xrpl-dev-portal/issues/1839
  uint32_t limit = kLimitDefault;
  std::optional<std::string> marker;
};

} // namespace rpc::spec::handlers::account_lines
