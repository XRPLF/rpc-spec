/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::mpt_holders {

inline constexpr uint32_t kLimitMin = 1;
inline constexpr uint32_t kLimitMax = 100;
inline constexpr uint32_t kLimitDefault = 50;

/**
 * @brief Input for the 'mpt_holders' RPC command.
 */
struct Input {
  std::string mptID;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  std::optional<std::string> marker;
  std::optional<uint32_t> limit;
};

} // namespace rpc::spec::handlers::mpt_holders
