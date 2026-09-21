/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>

namespace rpc::spec::handlers::server_info {

/**
 * @brief Value of the `k_backend_counters_key` field.
 */
inline constexpr auto kBackendCountersKey = "backend_counters";

/**
 * @brief Input for the 'server_info' RPC command.
 */
struct Input
{
    /**
     * @brief Value of the `backend_counters` field.
     */
    JsonBool backendCounters{false};
};

}  // namespace rpc::spec::handlers::server_info
