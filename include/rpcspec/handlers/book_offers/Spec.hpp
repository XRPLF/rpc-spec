/** @file */
#pragma once
// Shared constexpr spec for the 'book_offers' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/book_offers/Types.hpp>

#include <cstdint>
#include <string>

namespace rpc::spec::handlers::book_offers {

inline constexpr auto kSpec = RpcSpec{
    field(
        "taker_gets",
        required,
        type<JsonObject>,
        section(
            field(
                "currency",
                required,
                withCustomError(currency, RippledError::RpcDstAmtMalformed)
            ),
            field("issuer", withCustomError(issuer, RippledError::RpcDstIsrMalformed))
        )
    ),
    field(
        "taker_pays",
        required,
        type<JsonObject>,
        section(
            field(
                "currency",
                required,
                withCustomError(currency, RippledError::RpcSrcCurMalformed)
            ),
            field("issuer", withCustomError(issuer, RippledError::RpcSrcIsrMalformed))
        )
    ),
    // return INVALID_PARAMS if account format is wrong for "taker"
    field(
        "taker",
        withCustomError(account, RippledError::RpcInvalidParams, "Invalid field 'taker'.")
    ),
    field(
        "domain",
        withCustomError(
            type<std::string>, RippledError::RpcDomainMalformed, "Unable to parse domain."
        ),
        withCustomError(
            uint256Hex, RippledError::RpcDomainMalformed, "Unable to parse domain."
        )
    ),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
};

} // namespace rpc::spec::handlers::book_offers
