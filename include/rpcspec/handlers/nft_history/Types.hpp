/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nft_history {

inline constexpr uint32_t kLimitMin = 1;
inline constexpr uint32_t kLimitMax = 100;
inline constexpr uint32_t kLimitDefault = 50;

/**
 * @brief A struct to hold the marker data
 */
// TODO: this marker is same as account_tx, reuse in future
struct Marker {
    uint32_t ledger;
    uint32_t seq;
};

/**
 * @brief Input for the 'nft_history' RPC command.
 */
struct Input {
    xrpl::uint256 nftID;
    // You must use at least one of the following fields in your request:
    // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
    // `ledger` is unspecified when none of ledger_hash/ledger_index is given, so
    // the handler can choose between range mode (min/max) and the default ledger.
    LedgerSpecifier ledger;
    std::optional<int32_t> ledgerIndexMin;
    std::optional<int32_t> ledgerIndexMax;
    JsonBool binary{false};
    JsonBool forward{false};
    std::optional<uint32_t> limit;
    std::optional<Marker> marker;
};

} // namespace rpc::spec::handlers::nft_history
