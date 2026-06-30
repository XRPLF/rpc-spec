/** @file */
#pragma once

#include <chrono>
#include <optional>

namespace rpc::spec::handlers::ledger_index {

inline constexpr auto kDateFormat = "%Y-%m-%dT%TZ";

/**
 * @brief Input for the 'ledger_index' RPC command.
 */
struct Input
{
    std::optional<std::chrono::system_clock::time_point> date;
};

}  // namespace rpc::spec::handlers::ledger_index
