/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nft_offers_common {

inline constexpr uint32_t kLimitMin = 50;
inline constexpr uint32_t kLimitMax = 500;
inline constexpr uint32_t kLimitDefault = 250;

/**
 * @brief Input for the 'nft_buy_offers' / 'nft_sell_offers' RPC commands.
 */
struct Input {
  std::string nftID;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  uint32_t limit = kLimitDefault;
  std::optional<std::string> marker;
};

} // namespace rpc::spec::handlers::nft_offers_common
