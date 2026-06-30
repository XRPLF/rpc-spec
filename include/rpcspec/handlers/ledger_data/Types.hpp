/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace rpc::spec::handlers::ledger_data {

inline constexpr uint32_t kLimitBinary = 2048;
inline constexpr uint32_t kLimitJson = 256;

/**
 * @brief Represents the validated 'marker' field: either a uint256 key (normal traversal)
 * or a uint32_t sequence number (out-of-order / diff traversal).
 */
using MarkerValue = std::variant<xrpl::uint256, uint32_t>;

/**
 * @brief Input for the 'ledger_data' RPC command.
 *
 * @note `outOfOrder` is only for Clio, there is no document, traverse via seq diff (outOfOrder
 * implementation is copied from old rpc handler).
 * @note `marker` holds either a uint256 (string hex marker for normal page traversal) or
 *       a uint32_t (integer sequence for out-of-order / diff traversal). Use
 *       std::holds_alternative / std::get to distinguish in the handler.
 */
struct Input
{
    LedgerSpecifier ledger;
    bool binary = false;
    uint32_t limit = kLimitJson;        // max 256 for json ; 2048 for binary
    std::optional<MarkerValue> marker;  // nullopt = no marker; uint256 = normal; uint32 = diff/OOO
    bool outOfOrder = false;
    xrpl::LedgerEntryType type = xrpl::LedgerEntryType::ltANY;
};

}  // namespace rpc::spec::handlers::ledger_data
