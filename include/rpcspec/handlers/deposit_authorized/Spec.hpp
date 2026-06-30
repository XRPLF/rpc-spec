/** @file */
#pragma once
// Shared constexpr spec for the 'deposit_authorized' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/handlers/deposit_authorized/Types.hpp>

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>

namespace rpc::spec::handlers::deposit_authorized {

struct CredentialsArrayConverter {
    static constexpr std::string_view kName = "credentialsArray";
    using ValueType = std::optional<boost::json::array>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.present())
            return std::nullopt;
        boost::json::array arr;
        arr.reserve(f.arraySize());
        for (std::size_t i = 0; i < f.arraySize(); ++i)
            arr.push_back(boost::json::value{std::string{f.element(i).asString()}});
        return arr;
    }
};

inline constexpr auto credentialsArrayConv = CredentialsArrayConverter{};

inline constexpr auto kInputSpec = spec<Input>(
    field("source_account", &Input::sourceAccount, required, accountId),
    field("destination_account", &Input::destinationAccount, required, accountId),
    field("ledger_hash", &Input::ledgerHash, ledgerHashHex),
    field("ledger_index", &Input::ledgerIndex, ledgerIndexOpt),
    field("credentials", &Input::credentials, hex256Array, credentialsArrayConv)
);

} // namespace rpc::spec::handlers::deposit_authorized
