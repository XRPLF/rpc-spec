/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/LedgerTypes.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/account_objects/Types.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::account_objects {

/**
 * @brief Converts the account owned type field into its strongly-typed value.
 */
struct AccountOwnedTypeConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("accountOwnedType").
     */
    static constexpr std::string_view kName = "accountOwnedType";

    /**
     * @brief The value this converter produces (`xrpl::LedgerEntryType`).
     */
    using ValueType = xrpl::LedgerEntryType;

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
                rpc::kMalformedField, rpc::expectedFieldMessage(fieldView.key(), "string")}};
        }
        auto const entryType = accountOwnedLedgerTypeFromStr(std::string{fieldView.asString()});
        if (entryType == xrpl::ltANY)
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::invalidFieldMessage(fieldView.key())}};
        }
        return entryType;
    }
};

/**
 * @brief Converts the marker string field into its strongly-typed value.
 */
struct MarkerStringConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("markerString").
     */
    static constexpr std::string_view kName = "markerString";

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
        return std::string{fieldView.asString()};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: account owned type.
 */
inline constexpr auto accountOwnedTypeConv = AccountOwnedTypeConverter{};

/**
 * @brief Converter instance: marker string.
 */
inline constexpr auto markerStringConv = MarkerStringConverter{};
// NOLINTEND(readability-identifier-naming)

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
    field("type", &Input::type, accountOwnedTypeConv),
    field("marker", &Input::marker, accountMarker, markerStringConv),
    field("deletion_blockers_only", &Input::deletionBlockersOnly, jsonBoolStrict),
    field("sponsored", &Input::sponsored, jsonBoolStrict));

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1);

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

}  // namespace rpc::spec::handlers::account_objects
