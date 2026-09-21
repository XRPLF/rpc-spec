/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/nft_history/Types.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::nft_history {

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
    field("nft_id", &Input::nftID, required, asUint256),
    field("ledger_index_min", &Input::ledgerIndexMin, type<int64_t>, clampAs<int32_t>, int32Bound),
    field("ledger_index_max", &Input::ledgerIndexMax, type<int64_t>, clampAs<int32_t>, int32Bound),
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
    // Unlike account_tx, this command applies the strict bool check on both API versions.
    field("binary", &Input::binary, jsonBoolStrict),
    field("forward", &Input::forward, jsonBoolStrict));

// The spec is version-invariant; V2 exists only so versioned<> has both slots.
/**
 * @brief The API v2 spec, derived from `kInputSpecV1`.
 */
inline constexpr auto kInputSpecV2 = kInputSpecV1;

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

}  // namespace rpc::spec::handlers::nft_history
