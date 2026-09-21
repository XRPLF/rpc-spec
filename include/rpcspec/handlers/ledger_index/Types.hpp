/** @file */
#pragma once

#include <chrono>
#include <optional>

namespace rpc::spec::handlers::ledger_index {

/**
 * @brief Value of the `k_date_format` field.
 */
inline constexpr auto kDateFormat = "%Y-%m-%dT%TZ";

/**
 * @brief Input for the 'ledger_index' RPC command.
 */
struct Input
{
    /**
     * @brief Value of the `date` request field.
     */
    std::optional<std::chrono::system_clock::time_point> date;
};

}  // namespace rpc::spec::handlers::ledger_index
