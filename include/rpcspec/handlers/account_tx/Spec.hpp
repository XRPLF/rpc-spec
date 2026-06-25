/** @file */
#pragma once
// Shared constexpr spec for the 'account_tx' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// V1: account, ledger_hash, ledger_index, ledger_index_min/max, ctid, limit, marker, tx_type
// V2: V1 + binary + forward

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/detail/TxTypes.hpp>
#include <rpcspec/handlers/account_tx/Types.hpp>

#include <cstdint>
#include <expected>
#include <string>

namespace rpc::spec::handlers::account_tx {

// Validates tx_type against the runtime-generated set of known transaction type names
// (lowercase). Because the set comes from xrpl::TxFormats at runtime, we use a
// CustomValidator lambda rather than the consteval spec::oneOf factory.
// Returns the same error shape as the old validation::OneOf: "Invalid field '<key>'."
inline constexpr auto kTxTypeValidator = CustomValidator{[](auto const& f) -> MaybeError {
    if (!f.isString()) {
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
    auto const& validTypes = detail::txTypesInLowercase();
    auto const sv = f.asString();
    if (!validTypes.contains(std::string{sv})) {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams, "Invalid field '" + std::string{f.key()} + "'."
        }};
    }
    return {};
}};

inline constexpr auto kSpecV1 = RpcSpec{
    field("account", required, account),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    // Mirrors old `Type<int32_t>{}` behaviour: silently coerce overflow into the int32
    // range so the downstream `tag_invoke` cast (to int32_t) is well-defined.
    field("ledger_index_min", type<int64_t>, clampAs<int32_t>),
    field("ledger_index_max", type<int64_t>, clampAs<int32_t>),
    field("ctid", type<std::string>),
    field(
        "limit",
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})
    ),
    field(
        "marker",
        withCustomError(type<JsonObject>, rpc::RippledError::RpcInvalidParams, "invalidMarker"),
        ifType<JsonObject>(section(
            field("ledger", required, type<uint32_t>), field("seq", required, type<uint32_t>)
        ))
    ),
    field("tx_type", type<std::string>, toLower, kTxTypeValidator),
};

inline constexpr auto kSpecV2 =
    extend(kSpecV1, field("binary", type<bool>), field("forward", type<bool>));

} // namespace rpc::spec::handlers::account_tx
