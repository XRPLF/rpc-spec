/** @file */
#pragma once
// Shared constexpr spec for the 'mptoken_issuance_history' RPC command.
//
// Clio-only method: returns past transactions associated with an MPTokenIssuance,
// optionally filtered by an affected account and/or transaction type.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/TxTypes.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/mptoken_issuance_history/Types.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace rpc::spec::handlers::mptoken_issuance_history {

// Same shape as account_tx's tx_type check: the valid set comes from xrpl::TxFormats at
// runtime, so this is a CustomValidator rather than the consteval oneOf factory.
inline constexpr auto kTxTypeValidator = CustomValidator{[](auto const& f) -> MaybeError {
    if (!f.isString())
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};

    auto const& validTypes = txTypesInLowercase();
    if (!validTypes.contains(std::string{f.asString()}))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams, "Invalid field '" + std::string{f.key()} + "'."}};
    }
    return {};
}};

/** @brief Maps the -1 sentinel onto "unset", matching account_tx. */
struct Int32BoundConverter
{
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

struct MarkerConverter
{
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
inline constexpr auto int32Bound = Int32BoundConverter{};
inline constexpr auto markerConv = MarkerConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("mpt_issuance_id", &Input::mptIssuanceId, required, uint192Hex, asUint192),
    field("account", &Input::account, accountId),
    field("tx_type", &Input::transactionTypeInLowercase, toLower, kTxTypeValidator, asString),
    field("ledger_index_min", &Input::ledgerIndexMin, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ledger_index_max", &Input::ledgerIndexMax, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("binary", &Input::binary, jsonBoolStrict),
    field("forward", &Input::forward, jsonBoolStrict),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32),
    field(
        "marker",
        &Input::marker,
        withCustomError(type<JsonObject>, rpc::RippledError::RpcInvalidParams, "invalidMarker"),
        ifType<JsonObject>(section(
            field("ledger", required, type<uint32_t>),
            field("seq", required, type<uint32_t>))),
        markerConv));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::mptoken_issuance_history
