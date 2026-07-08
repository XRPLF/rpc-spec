/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Ledger.hpp>

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
struct Input
{
    LedgerSpecifier ledger;
    xrpl::uint192 mptID;
    std::optional<xrpl::AccountID> marker;
    uint32_t limit;
};

}  // namespace rpc::spec::handlers::mpt_holders
