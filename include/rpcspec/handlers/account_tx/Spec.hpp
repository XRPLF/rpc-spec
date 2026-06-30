/** @file */
#pragma once
// Shared constexpr spec for the 'account_tx' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// V1: account, ledger_hash, ledger_index, ledger_index_min/max, ctid, limit, marker, tx_type
// V2: V1 + binary + forward

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/detail/TxTypes.hpp>
#include <rpcspec/handlers/account_tx/Types.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
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

struct LedgerIndexSpecConverter {
    static constexpr std::string_view kName = "ledgerIndex";
    using ValueType = LedgerIndexSpec;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isUint32())
            return LedgerIndexSpec{.index = f.asUint32(), .usingValidated = false};
        if (f.isInt64())  // numeric, out of uint32 range → fall back to latest validated
            return LedgerIndexSpec{.index = std::nullopt, .usingValidated = true};
        if (!f.isString())
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, "Invalid field 'ledger_index', not string or number."
            }};
        auto const sv = f.asString();
        if (sv == "validated" || sv == "current" || sv == "closed")
            return LedgerIndexSpec{.index = std::nullopt, .usingValidated = true};
        uint32_t out = 0;
        auto const* const begin = sv.data();
        auto const* const end = sv.data() + sv.size();
        if (auto const [p, ec] = std::from_chars(begin, end, out); ec == std::errc{} && p == end)
            return LedgerIndexSpec{.index = out, .usingValidated = false};
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams, "Invalid field 'ledger_index', not string or number."
        }};
    }
};

struct Int32BoundConverter {
    static constexpr std::string_view kName = "int32";
    using ValueType = std::optional<int32_t>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const v = static_cast<int32_t>(f.asInt64());
        if (v == -1)
            return std::optional<int32_t>{std::nullopt};
        return std::optional<int32_t>{v};
    }
};

struct MarkerConverter {
    static constexpr std::string_view kName = "marker";
    using ValueType = Marker;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        return Marker{.ledger = f.child("ledger").asUint32(), .seq = f.child("seq").asUint32()};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto ledgerIndexSpec = LedgerIndexSpecConverter{};
inline constexpr auto int32Bound = Int32BoundConverter{};
inline constexpr auto markerConv = MarkerConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpecV1 = spec<Input>(
    field("account", &Input::account, required, accountId),
    field("ledger_hash", &Input::ledgerHash, ledgerHashHex),
    field("ledger_index", &Input::ledgerIndex, ledgerIndexSpec),
    field("ledger_index_min", &Input::ledgerIndexMin, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ledger_index_max", &Input::ledgerIndexMax, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ctid", type<std::string>),  // validated but not stored (no Input member)
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32
    ),
    field(
        "marker",
        &Input::marker,
        withCustomError(type<JsonObject>, rpc::RippledError::RpcInvalidParams, "invalidMarker"),
        ifType<JsonObject>(section(
            field("ledger", required, type<uint32_t>), field("seq", required, type<uint32_t>)
        )),
        markerConv
    ),
    // binary/forward exist on Input for both versions; V1 coerces leniently (they are
    // not part of the V1 schema), V2 retightens them to a strict bool below.
    field("binary", &Input::binary, jsonBool),
    field("forward", &Input::forward, jsonBool),
    field("tx_type", &Input::transactionTypeInLowercase, toLower, kTxTypeValidator, asString)
);

inline constexpr auto kInputSpecV2 = extend(
    kInputSpecV1,
    field("binary", &Input::binary, jsonBoolStrict),
    field("forward", &Input::forward, jsonBoolStrict)
);

} // namespace rpc::spec::handlers::account_tx
