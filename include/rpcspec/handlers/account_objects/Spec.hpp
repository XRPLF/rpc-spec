/** @file */
#pragma once
// Shared constexpr spec for the 'account_objects' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/account_objects/Types.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::account_objects {

struct AccountOwnedTypeConverter {
    static constexpr std::string_view kName = "accountOwnedType";
    using ValueType = xrpl::LedgerEntryType;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not string."
            }};
        }
        auto const t = detail::accountOwnedLedgerTypeFromStr(std::string{f.asString()});
        if (t == xrpl::ltANY) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "'."
            }};
        }
        return t;
    }
};

struct MarkerStringConverter {
    static constexpr std::string_view kName = "markerString";
    using ValueType = std::string;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        return std::string{f.asString()};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto accountOwnedTypeConv = AccountOwnedTypeConverter{};
inline constexpr auto markerStringConv = MarkerStringConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("account", &Input::account, required, accountId),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32
    ),
    field("type", &Input::type, accountOwnedTypeConv),
    field("marker", &Input::marker, accountMarker, markerStringConv),
    field("deletion_blockers_only", &Input::deletionBlockersOnly, jsonBoolStrict)
);

}  // namespace rpc::spec::handlers::account_objects
