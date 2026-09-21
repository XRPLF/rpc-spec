/** @file */
#pragma once
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
/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kTxTypeValidator = CustomValidator{[](auto const& fieldView) -> MaybeError {
    if (not fieldView.isString())
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};

    auto const& validTypes = txTypesInLowercase();
    if (not validTypes.contains(std::string{fieldView.asString()}))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field '" + std::string{fieldView.key()} + "'."}};
    }
    return {};
}};

/**
 * @brief Maps the -1 sentinel onto "unset", matching account_tx.
 */
struct Int32BoundConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("int32").
     */
    static constexpr std::string_view kName = "int32";

    /**
     * @brief The value this converter produces (`std::optional<int32_t>`).
     */
    using ValueType = std::optional<int32_t>;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        auto const value = static_cast<int32_t>(fieldView.asInt64());
        if (value == -1)
            return std::optional<int32_t>{std::nullopt};
        return std::optional<int32_t>{value};
    }
};

/**
 * @brief Converts the marker field into its strongly-typed value.
 */
struct MarkerConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("marker").
     */
    static constexpr std::string_view kName = "marker";

    /**
     * @brief The value this converter produces (`Marker`).
     */
    using ValueType = Marker;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        return Marker{
            .ledger = fieldView.child("ledger").asUint32(),
            .seq = fieldView.child("seq").asUint32()};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: int32 bound.
 */
inline constexpr auto int32Bound = Int32BoundConverter{};

/**
 * @brief Converter instance: marker.
 */
inline constexpr auto markerConv = MarkerConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
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

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/**
 * @brief ADL hook: resolve the versioned spec from the Input type.
 *
 * @return A reference to this handler's `kSpec`, for `HandlerFor` to select a version from.
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::mptoken_issuance_history
