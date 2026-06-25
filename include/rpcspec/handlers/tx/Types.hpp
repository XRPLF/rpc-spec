/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::tx {

/**
 * @brief Input for the 'tx' RPC command.
 */
struct Input {
  std::optional<std::string> transaction;
  std::optional<std::string> ctid;
  bool binary = false;
  std::optional<uint32_t> minLedger;
  std::optional<uint32_t> maxLedger;
};

} // namespace rpc::spec::handlers::tx
