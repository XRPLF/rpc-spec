/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::account_offers {

/**
 * @brief Converts the account marker str field into its strongly-typed value.
 */
struct AccountMarkerStrConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("accountMarker").
     */
    static constexpr std::string_view kName = "accountMarker";

    /**
     * @brief The value this converter produces (`std::string`).
     */
    using ValueType = std::string;

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
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotString"}};
        }
        auto const sv = fieldView.asString();
        auto const malformed = [&] {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::malformedCursorMessage(fieldView.key())}};
        };
        auto const commaPos = sv.find(',');
        if (commaPos == std::string_view::npos)
            return malformed();
        auto const hexPart = std::string{sv.substr(0, commaPos)};
        auto const hintPart = sv.substr(commaPos + 1);
        xrpl::uint256 index;
        if (not index.parseHex(hexPart.c_str()))
            return malformed();
        uint64_t hint = 0;
        auto const [ptr, ec] =
            std::from_chars(hintPart.data(), hintPart.data() + hintPart.size(), hint);
        if (ec != std::errc() or ptr != hintPart.data() + hintPart.size())
            return malformed();
        return std::string{sv};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Converter instance: account marker str.
 */
inline constexpr auto accountMarkerStr = AccountMarkerStrConverter{};

/**
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        defaultTo(kLimitDefault),
        asUint32),
    field("marker", &Input::marker, accountMarkerStr),
    field("ledger", deprecated),
    field("strict", deprecated));

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

}  // namespace rpc::spec::handlers::account_offers
