/** @file */
#pragma once

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
  std::string issuer;
  std::optional<uint32_t> nftTaxon;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  std::optional<std::string> marker;
  std::optional<uint32_t> limit;
};

} // namespace rpc::spec::handlers::nfts_by_issuer
