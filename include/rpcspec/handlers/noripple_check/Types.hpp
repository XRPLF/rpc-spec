/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

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
struct Input
{
    LedgerSpecifier ledger;
    xrpl::AccountID account;
    bool roleGateway = false;
    uint32_t limit = kLimitDefault;
    bool transactions = false;
};

}  // namespace rpc::spec::handlers::noripple_check
