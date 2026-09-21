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

/**
 * @brief Value of the `k_limit_binary` field.
 */
inline constexpr uint32_t kLimitBinary = 2048;

/**
 * @brief Value of the `k_limit_json` field.
 */
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
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `binary` request field.
     */
    bool binary = false;

    /**
     * @note nullopt = not supplied. The default depends on `binary` (kLimitBinary vs
     * kLimitJson), which a per-field spec default cannot express, so the handler applies
     * it. Mirrors xrpld's `maxLimit = rpc::tuning::pageLength(isBinary)`.
     */
    std::optional<uint32_t> limit;

    /**
     * @brief Value of the `marker` request field.
     */
    std::optional<MarkerValue> marker;  // nullopt = no marker; uint256 = normal; uint32 = diff/OOO

    /**
     * @brief Value of the `out_of_order` request field.
     */
    bool outOfOrder = false;

    /**
     * @brief Value of the `type` request field.
     */
    xrpl::LedgerEntryType type = xrpl::LedgerEntryType::ltANY;
};

}  // namespace rpc::spec::handlers::ledger_data
