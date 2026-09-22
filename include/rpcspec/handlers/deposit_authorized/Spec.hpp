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

/**
 * @brief Converts the credentials array field into its strongly-typed value.
 */
struct CredentialsArrayConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("credentialsArray").
     */
    static constexpr std::string_view kName = "credentialsArray";

    /**
     * @brief The value this converter produces (`std::optional<std::vector<xrpl::uint256>>`).
     */
    using ValueType = std::optional<std::vector<xrpl::uint256>>;

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
        if (not fieldView.present())
            return std::nullopt;
        // hex256Array already validated each element is a well-formed uint256 hex string, so
        // this cannot fail — uint256FromValidated states that precondition rather than
        // discarding parseHex's [[nodiscard]] result, matching every other handler here.
        auto const size = fieldView.arraySize();
        std::vector<xrpl::uint256> out;
        out.reserve(size);
        for (auto i = 0uz; i < size; ++i)
        {
            out.push_back(
                detail::uint256FromValidated(std::string{fieldView.element(i).asString()}));
        }
        return out;
    }
};

/**
 * @brief Converter instance: credentials array.
 */
inline constexpr auto credentialsArrayConv = CredentialsArrayConverter{};

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
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
 *
 * @return A reference to this handler's `kSpec`, for `HandlerFor` to select a version from.
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::deposit_authorized
