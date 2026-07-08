/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::nft_info {

/**
 * @brief Input for the 'nft_info' RPC command.
 */
struct Input
{
    LedgerSpecifier ledger;
    xrpl::uint256 nftID;
};

}  // namespace rpc::spec::handlers::nft_info
