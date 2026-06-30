/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nfts_by_issuer {

inline constexpr uint32_t kLimitMin = 1;
inline constexpr uint32_t kLimitMax = 100;
inline constexpr uint32_t kLimitDefault = 50;

/**
 * @brief Input for the 'nfts_by_issuer' RPC command.
 */
struct Input {
    xrpl::AccountID issuer;
    std::optional<uint32_t> nftTaxon;
    LedgerSpecifier ledger;
    std::optional<xrpl::uint256> marker;
    uint32_t limit = kLimitDefault;  /**< Clamped to [kLimitMin, kLimitMax] */
};

} // namespace rpc::spec::handlers::nfts_by_issuer
