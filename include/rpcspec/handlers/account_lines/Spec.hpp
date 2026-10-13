/** @file */
#pragma once

#include <rpcspec/Account.hpp>
#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_lines/Types.hpp>

#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>

namespace rpc::spec::handlers::account_lines {

/**
 * @brief Converts an account field: deferred on xrpld, `actMalformed` for any failure on Clio.
 */
struct LinesAccountConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("account").
     */
    static constexpr std::string_view kName = "account";

    /**
     * @brief The value this converter produces (`DeferredAccountId`).
     */
    using ValueType = DeferredAccountId;

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
        if constexpr (kIsXrpldBuild)
        {
            return deferredAccountId.parse(fieldView);
        }
        else
        {
            auto id = accountIdActMalformed.parse(fieldView);
            if (not id.has_value())
                return std::unexpected{std::move(id).error()};
            return ValueType{*id};
        }
    }
};

/**
 * @brief Converts the `ignore_default` field: any value coerced to bool on xrpld, a bool on Clio.
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
        if constexpr (kIsXrpldBuild)
        {
            auto const coerced = jsonBool.parse(fieldView);
            return coerced.has_value() and coerced->value;
        }
        else
        {
            if (not fieldView.isBool())
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
            return fieldView.asBool();
        }
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: the `account` and `peer` fields.
 */
inline constexpr auto linesAccount = LinesAccountConverter{};

/**
 * @brief Converter instance: the `ignore_default` field.
 */
inline constexpr auto asBool = AsBoolConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, linesAccount),
    field("peer", &Input::peer, emptyAsAbsent(linesAccount)),
    field("ignore_default", &Input::ignoreDefault, ifServerClio(type<bool>), asBool),
    field(
        "limit",
        &Input::limit,
        ifServerXrpld(nullAs(kLimitDefault)),
        ifServerXrpld(
            withCustomError(
                type<uint32_t>,
                rpc::XrpldError::RpcInvalidParams,
                "Invalid field 'limit', not unsigned integer."),
            withCustomError(
                min(uint32_t{1}),
                rpc::XrpldError::RpcInvalidParams,
                "Invalid field 'limit'.")),
        ifServerClio(type<uint32_t>, min(uint32_t{1})),
        ifServerClio(clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax})),
        defaultTo(kLimitDefault),
        asUint32),
    field("marker", &Input::marker, accountMarkerStrInvalidParams),
    field("ledger", ifServerClio(deprecated)),
    field("peer_index", ifServerClio(deprecated)));

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
