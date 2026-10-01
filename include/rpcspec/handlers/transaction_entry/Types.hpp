/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <expected>

namespace rpc::spec::handlers::transaction_entry {

/**
 * @brief Why a request's `tx_hash` could not be read as a hash.
 */
enum class TxHashError : std::uint8_t {
    Missing,   ///< The request has no `tx_hash`.
    Malformed  ///< `tx_hash` is not a hex-encoded uint256.
};

/**
 * @brief Input for the 'transaction_entry' RPC command.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `tx_hash` request field, or why there is none.
     *
     * Clio's spec rejects a missing or malformed `tx_hash`, so Clio always gets a hash. xrpld
     * reports both only after it has resolved the ledger, so its spec leaves them to the handler.
     */
    std::expected<xrpl::uint256, TxHashError> txHash = std::unexpected{TxHashError::Missing};
};

}  // namespace rpc::spec::handlers::transaction_entry
