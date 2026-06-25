/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::ledger_data {

inline constexpr uint32_t kLimitBinary = 2048;
inline constexpr uint32_t kLimitJson = 256;

/**
 * @brief Input for the 'ledger_data' RPC command.
 *
 * @note `outOfOrder` is only for Clio, there is no document, traverse via seq diff (outOfOrder
 * implementation is copied from old rpc handler)
 */
struct Input {
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    bool binary = false;
    uint32_t limit = kLimitJson;  // max 256 for json ; 2048 for binary
    std::optional<xrpl::uint256> marker;
    std::optional<uint32_t> diffMarker;
    bool outOfOrder = false;
    xrpl::LedgerEntryType type = xrpl::LedgerEntryType::ltANY;
};

} // namespace rpc::spec::handlers::ledger_data
