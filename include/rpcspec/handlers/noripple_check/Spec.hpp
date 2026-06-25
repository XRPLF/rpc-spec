/** @file */
#pragma once
// Shared constexpr spec for the 'noripple_check' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/handlers/noripple_check/Types.hpp>

#include <cstdint>

namespace rpc::spec::handlers::noripple_check {

inline constexpr auto kSpecV1 = RpcSpec{
    field("account", required, account),
    field(
        "role",
        required,
        withCustomError(
            oneOf<std::string>("gateway", "user"),
            rpc::RippledError::RpcInvalidParams,
            "role field is invalid"
        )
    ),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
};

inline constexpr auto kSpecV2 = extend(kSpecV1, field("transactions", type<bool>));

} // namespace rpc::spec::handlers::noripple_check
