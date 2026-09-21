/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
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
        // hex256Array already validated each element is a well-formed uint256 hex string, so
        // this cannot fail — uint256FromValidated states that precondition rather than
        // discarding parseHex's [[nodiscard]] result, matching every other handler here.
        auto const size = f.arraySize();
        std::vector<xrpl::uint256> out;
        out.reserve(size);
        for (std::size_t i = 0; i < size; ++i)
            out.push_back(detail::uint256FromValidated(std::string{f.element(i).asString()}));
        return out;
    }
};

inline constexpr auto credentialsArrayConv = CredentialsArrayConverter{};

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("source_account", &Input::sourceAccount, required, accountId),
    field("destination_account", &Input::destinationAccount, required, accountId),
    field("credentials", &Input::credentials, hex256Array, credentialsArrayConv));

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/**
 * @brief ADL hook: resolve the versioned spec from the Input type.
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::deposit_authorized
