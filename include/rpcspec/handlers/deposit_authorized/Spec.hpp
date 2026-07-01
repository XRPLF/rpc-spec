/** @file */
#pragma once
// Shared constexpr spec for the 'deposit_authorized' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/deposit_authorized/Types.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace rpc::spec::handlers::deposit_authorized {

struct CredentialsArrayConverter
{
    static constexpr std::string_view kName = "credentialsArray";
    using ValueType = std::optional<std::vector<xrpl::uint256>>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.present())
            return std::nullopt;
        // hex256Array already validated each element is a well-formed uint256 hex
        // string, so parseHex here cannot fail.
        std::vector<xrpl::uint256> out;
        out.reserve(f.arraySize());
        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            xrpl::uint256 hash;
            hash.parseHex(std::string{f.element(i).asString()}.c_str());
            out.push_back(hash);
        }
        return out;
    }
};

inline constexpr auto credentialsArrayConv = CredentialsArrayConverter{};

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("source_account", &Input::sourceAccount, required, accountId),
    field("destination_account", &Input::destinationAccount, required, accountId),
    field("credentials", &Input::credentials, hex256Array, credentialsArrayConv));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::deposit_authorized
