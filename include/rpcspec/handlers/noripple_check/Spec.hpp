/** @file */
#pragma once
// Shared constexpr spec for the 'noripple_check' RPC command.
// Single source of truth — both Clio and xrpld include this file.

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

struct RoleGatewayConverter
{
    static constexpr std::string_view kName = "role";
    using ValueType = bool;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        return f.asString() == "gateway";
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto roleGateway = RoleGatewayConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field(
        "role",
        &Input::roleGateway,
        required,
        withCustomError(
            oneOf<std::string>("gateway", "user"),
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

inline constexpr auto kInputSpecV2 =
    extend(kInputSpecV1, field("transactions", &Input::transactions, jsonBoolStrict));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::noripple_check
