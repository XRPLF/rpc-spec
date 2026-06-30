/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nft_info {

/**
 * @brief Input for the 'nft_info' RPC command.
 */
struct Input {
    xrpl::uint256 nftID;
    LedgerSpecifier ledger;
};

}  // namespace rpc::spec::handlers::nft_info
