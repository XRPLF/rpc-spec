/** @file */
#pragma once

namespace rpc::spec::handlers::server_info {

inline constexpr auto kBackendCountersKey = "backend_counters";

/**
 * @brief Input for the 'server_info' RPC command.
 */
struct Input {
    bool backendCounters = false;
};

} // namespace rpc::spec::handlers::server_info
