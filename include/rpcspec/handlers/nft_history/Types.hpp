/** @file */
#pragma once

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
    std::string nftID;
    // You must use at least one of the following fields in your request:
    // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    std::optional<int32_t> ledgerIndexMin;
    std::optional<int32_t> ledgerIndexMax;
    bool binary = false;
    bool forward = false;
    std::optional<uint32_t> limit;
    std::optional<Marker> marker;
};

} // namespace rpc::spec::handlers::nft_history
