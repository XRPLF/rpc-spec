/** @file */
#pragma once
// Shared constexpr spec for the 'nft_history' RPC command.
// Single source of truth — both Clio and rippled include this file.

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

struct Uint256Converter
{
    static constexpr std::string_view kName = "uint256";
    using ValueType = xrpl::uint256;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        if (!f.isString())
            return err();
        xrpl::uint256 out;
        if (!out.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return out;
    }
};

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
inline constexpr auto asUint256 = Uint256Converter{};
inline constexpr auto int32Bound = Int32BoundConverter{};
inline constexpr auto markerConv = MarkerConverter{};
// NOLINTEND(readability-identifier-naming)

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
    // binary/forward exist on Input for both versions; V1 coerces leniently, V2 retightens below.
    field("binary", &Input::binary, jsonBool),
    field("forward", &Input::forward, jsonBool));

inline constexpr auto kInputSpecV2 = extend(
    kInputSpecV1,
    field("binary", &Input::binary, jsonBoolStrict),
    field("forward", &Input::forward, jsonBoolStrict));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::nft_history
