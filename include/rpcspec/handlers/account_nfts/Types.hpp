/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>

namespace rpc::spec::handlers::account_nfts {

inline constexpr uint32_t kLimitMin = 20;
inline constexpr uint32_t kLimitMax = 400;
inline constexpr uint32_t kLimitDefault = 100;

/**
 * @brief Input for the 'account_nfts' RPC command.
 */
struct Input {
    xrpl::AccountID account;
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    uint32_t limit = kLimitDefault;  ///< Clamped to [kLimitMin, kLimitMax]
    std::optional<xrpl::uint256> marker;
};

}  // namespace rpc::spec::handlers::account_nfts
