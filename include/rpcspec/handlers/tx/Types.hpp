/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::tx {

/**
 * @brief Input for the 'tx' RPC command.
 */
struct Input {
    std::optional<xrpl::uint256> transaction;
    std::optional<std::string> ctid;  /**< Opaque CTID hex token (not a 256-bit hash); decoded downstream. */
    JsonBool binary{false};
    std::optional<uint32_t> minLedger;
    std::optional<uint32_t> maxLedger;
};

} // namespace rpc::spec::handlers::tx
