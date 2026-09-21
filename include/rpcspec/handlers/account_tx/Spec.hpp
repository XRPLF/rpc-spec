/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/TxTypes.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/VersionedSpec.hpp>
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
/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kTxTypeValidator = CustomValidator{[](auto const& fieldView) -> MaybeError {
    if (not fieldView.isString())
    {
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
    auto const& validTypes = txTypesInLowercase();
    auto const sv = fieldView.asString();
    if (not validTypes.contains(std::string{sv}))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field '" + std::string{fieldView.key()} + "'."}};
    }
    return {};
}};

/**
 * @brief Converts the int32 bound field into its strongly-typed value.
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

/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kDelegateValidator = CustomValidator{[](auto const& fieldView) -> MaybeError {
    if (not fieldView.isObject())
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotObject"}};
    }

    auto const filterView = fieldView.child("delegate_filter");
    if (not filterView.present())
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Field 'delegate_filter' is required but missing."}};
    }

    if (not filterView.isString() or
        (filterView.asString() != "actor" and filterView.asString() != "authorizer"))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Field 'delegate_filter' value must be 'actor' or 'authorizer'."}};
    }

    auto const counterPartyView = fieldView.child("counter_party");
    if (counterPartyView.present())
    {
        if (auto const err = AccountFormat::verify(counterPartyView); not err.has_value())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcActMalformed,
                "Field 'counter_party' value must be a valid account."}};
        }
    }

    return {};
}};

/**
 * @brief Builds the DelegateFilter once kDelegateValidator has accepted the object.
 */
struct DelegateConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("delegate").
     */
    static constexpr std::string_view kName = "delegate";

    /**
     * @brief The value this converter produces (`DelegateFilter`).
     */
    using ValueType = DelegateFilter;

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
        DelegateFilter out{};
        out.delegateType = fieldView.child("delegate_filter").asString() == "actor"
            ? DelegateFilter::Role::Actor
            : DelegateFilter::Role::Authorizer;

        auto const counterPartyView = fieldView.child("counter_party");
        if (counterPartyView.present())
            out.counterParty = std::string{counterPartyView.asString()};

        return out;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Converter instance: delegate.
 */
inline constexpr auto delegateConv = DelegateConverter{};

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
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field("ledger_index_min", &Input::ledgerIndexMin, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ledger_index_max", &Input::ledgerIndexMax, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ctid", type<std::string>),  // validated but not stored (no Input member)
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
        markerConv),
    // binary/forward exist on Input for both versions; V1 coerces leniently (they are
    // not part of the V1 schema), V2 retightens them to a strict bool below.
    field("binary", &Input::binary, jsonBool),
    field("forward", &Input::forward, jsonBool),
    field("tx_type", &Input::transactionTypeInLowercase, toLower, kTxTypeValidator, asString),
    field("mpt_issuance_id", &Input::mptIssuanceId, asUint192),
    field("delegate", &Input::delegateFilter, kDelegateValidator, delegateConv));

/**
 * @brief The API v2 spec, derived from `kInputSpecV1`.
 */
inline constexpr auto kInputSpecV2 = extend(
    kInputSpecV1,
    field("binary", &Input::binary, jsonBoolStrict),
    field("forward", &Input::forward, jsonBoolStrict));

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

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

}  // namespace rpc::spec::handlers::account_tx
