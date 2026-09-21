/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/JsonBool.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::tx {

/**
 * @brief Input for the 'tx' RPC command.
 */
struct Input
{
    /**
     * @brief Value of the `transaction` request field.
     */
    std::optional<xrpl::uint256> transaction;

    std::optional<std::string>
        ctid;  ///< Opaque CTID hex token (not a 256-bit hash); decoded downstream.

    /**
     * @brief Value of the `binary` request field.
     */
    JsonBool binary{false};

    /**
     * @brief Value of the `min_ledger` request field.
     */
    std::optional<uint32_t> minLedger;

    /**
     * @brief Value of the `max_ledger` request field.
     */
    std::optional<uint32_t> maxLedger;
};

}  // namespace rpc::spec::handlers::tx
