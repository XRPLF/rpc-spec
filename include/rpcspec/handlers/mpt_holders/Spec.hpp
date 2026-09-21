/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::mpt_holders {

/**
 * @brief Converts the account id hex field into its strongly-typed value.
 */
struct AccountIdHexConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("uint160Hex").
     */
    static constexpr std::string_view kName = "uint160Hex";

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
        auto const err = [&] {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::malformedFieldMessage(fieldView.key())}};
        };
        if (not fieldView.isString())
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::notStringFieldMessage(fieldView.key())}};
        }
        xrpl::AccountID out;
        if (not out.parseHex(std::string{fieldView.asString()}.c_str()))
            return err();
        return out;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: account id hex.
 */
inline constexpr auto accountIdHex = AccountIdHexConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("mpt_issuance_id", &Input::mptID, required, asUint192),
    field("marker", &Input::marker, accountIdHex),
    field("accounts", &Input::accounts, accountIdArray<kMaxAccounts>, asAccountIdVec),
    // No defaultTo: the handler has to tell a client-supplied limit from an absent one,
    // because `accounts` cannot be combined with paging (see Input::accounts).
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32));

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

}  // namespace rpc::spec::handlers::mpt_holders
