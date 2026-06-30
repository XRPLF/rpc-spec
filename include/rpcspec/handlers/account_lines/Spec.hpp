/** @file */
#pragma once
// Shared constexpr spec for the 'account_lines' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/account_lines/Types.hpp>
#include <rpcspec/detail/XrplParse.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::account_lines {

struct AccountIdActMalformedConverter {
    static constexpr std::string_view kName = "accountActMalformed";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcActMalformed}};
        auto id = detail::accountFromStringStrict(std::string{f.asString()});
        if (!id)
            return std::unexpected{rpc::Status{rpc::RippledError::RpcActMalformed}};
        return *id;
    }
};

struct AsBoolConverter {
    static constexpr std::string_view kName = "bool";
    using ValueType = bool;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isBool())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return f.asBool();
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto accountIdActMalformed = AccountIdActMalformedConverter{};
inline constexpr auto asBool = AsBoolConverter{};
// NOLINTEND(readability-identifier-naming)

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
        asUint32
    ),
    field("marker", &Input::marker, accountMarker, asString),
    field("ledger", deprecated),
    field("peer_index", deprecated)
);

}  // namespace rpc::spec::handlers::account_lines
