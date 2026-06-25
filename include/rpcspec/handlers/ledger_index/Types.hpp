/** @file */
#pragma once

#include <optional>
#include <string>

namespace rpc::spec::handlers::ledger_index {

inline constexpr auto kDateFormat = "%Y-%m-%dT%TZ";

/**
 * @brief Input for the 'ledger_index' RPC command.
 */
struct Input {
  std::optional<std::string> date;
};

} // namespace rpc::spec::handlers::ledger_index
