/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nft_info {

/**
 * @brief Input for the 'nft_info' RPC command.
 */
struct Input {
  std::string nftID;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
};

} // namespace rpc::spec::handlers::nft_info
