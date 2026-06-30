/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::noripple_check {

inline constexpr uint32_t kLimitMin = 1;
inline constexpr uint32_t kLimitMax = 500;
inline constexpr uint32_t kLimitDefault = 300;

/**
 * @brief Input for the 'noripple_check' RPC command.
 */
struct Input {
    xrpl::AccountID account;
    bool roleGateway = false;
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    uint32_t limit = kLimitDefault;
    bool transactions = false;
};

} // namespace rpc::spec::handlers::noripple_check
