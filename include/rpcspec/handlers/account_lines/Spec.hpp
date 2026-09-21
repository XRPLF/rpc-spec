/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/account_lines/Types.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::account_lines {

/**
 * @brief Converts the account id act malformed field into its strongly-typed value.
 */
struct AccountIdActMalformedConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("accountActMalformed").
     */
    static constexpr std::string_view kName = "accountActMalformed";

    /**
     * @brief The value this converter produces (`xrpl::AccountID`).
     */
    using ValueType = xrpl::AccountID;

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
            return std::unexpected{rpc::Status{rpc::RippledError::RpcActMalformed}};
        auto id = detail::accountFromStringStrict(std::string{fieldView.asString()});
        if (not id.has_value())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcActMalformed}};
        return *id;
    }
};

/**
 * @brief Converts the as bool field into its strongly-typed value.
 */
struct AsBoolConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("bool").
     */
    static constexpr std::string_view kName = "bool";

    /**
     * @brief The value this converter produces (`bool`).
     */
    using ValueType = bool;

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
        if (not fieldView.isBool())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return fieldView.asBool();
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: account id act malformed.
 */
inline constexpr auto accountIdActMalformed = AccountIdActMalformedConverter{};

/**
 * @brief Converter instance: as bool.
 */
inline constexpr auto asBool = AsBoolConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountIdActMalformed),
    field("peer", &Input::peer, accountIdActMalformed),
    field("ignore_default", &Input::ignoreDefault, type<bool>, asBool),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        defaultTo(kLimitDefault),
        asUint32),
    field("marker", &Input::marker, accountMarker, asString),
    field("ledger", deprecated),
    field("peer_index", deprecated));

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

}  // namespace rpc::spec::handlers::account_lines
