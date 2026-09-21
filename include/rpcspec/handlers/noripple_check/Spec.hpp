/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/noripple_check/Types.hpp>

#include <cstdint>
#include <expected>
#include <string_view>

namespace rpc::spec::handlers::noripple_check {

/**
 * @brief Converts the role gateway field into its strongly-typed value.
 */
struct RoleGatewayConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("role").
     */
    static constexpr std::string_view kName = "role";

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
        return fieldView.asString() == "gateway";
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Value of the `role` request field.
 */
inline constexpr auto roleGateway = RoleGatewayConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The API v1 spec; see `kInputSpecV2` for the v2 differences.
 */
inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field(
        "role",
        &Input::roleGateway,
        required,
        withCustomError(
            oneOf("gateway", "user"),
            rpc::RippledError::RpcInvalidParams,
            "role field is invalid"),
        roleGateway),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        defaultTo(kLimitDefault),
        asUint32),
    field("transactions", &Input::transactions, jsonBool));

/**
 * @brief The API v2 spec, derived from `kInputSpecV1`.
 */
inline constexpr auto kInputSpecV2 =
    extend(kInputSpecV1, field("transactions", &Input::transactions, jsonBoolStrict));

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

}  // namespace rpc::spec::handlers::noripple_check
