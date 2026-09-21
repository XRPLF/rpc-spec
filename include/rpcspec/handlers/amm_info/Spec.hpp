/** @file */
#pragma once

#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/amm_info/Types.hpp>

#include <expected>
#include <stdexcept>
#include <string>

namespace rpc::spec::handlers::amm_info {

// field is already confirmed to be a string (inside ifType<std::string>)
/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kStringIssueValidator =
    CustomValidator{[](auto const& fieldView) -> MaybeError {
        try
        {
            xrpl::issueFromJson(std::string{fieldView.asString()});
        }
        catch (std::runtime_error const&)
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
        }
        return {};
    }};

/**
 * @brief Converts the issue field into its strongly-typed value.
 */
struct IssueConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("issue").
     */
    static constexpr std::string_view kName = "issue";

    /**
     * @brief The value this converter produces (`xrpl::Issue`).
     */
    using ValueType = xrpl::Issue;

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
        if (fieldView.isString())
        {
            try
            {
                return xrpl::issueFromJson(std::string{fieldView.asString()});
            }
            catch (std::runtime_error const&)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
            }
        }
        if (fieldView.isObject())
        {
            try
            {
                auto const currSv = fieldView.child("currency").asString();
                xrpl::Currency currency{};
                if (not xrpl::toCurrency(currency, std::string{currSv}))
                    return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
                if (xrpl::isXRP(currency))
                    return xrpl::xrpIssue();
                auto const issuerSv = fieldView.child("issuer").asString();
                xrpl::AccountID issuer{};
                if (not xrpl::toIssuer(issuer, std::string{issuerSv}))
                    return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
                return xrpl::Issue{currency, issuer};
            }
            catch (...)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
            }
        }
        return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Converter instance: issue.
 */
inline constexpr auto issueConv = IssueConverter{};

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "asset",
        &Input::issue1,
        withCustomError(type<std::string, JsonObject>, rpc::RippledError::RpcIssueMalformed),
        ifType<std::string>(kStringIssueValidator),
        ifType<JsonObject>(withCustomError(currencyIssue, rpc::RippledError::RpcIssueMalformed)),
        issueConv),
    field(
        "asset2",
        &Input::issue2,
        withCustomError(type<std::string, JsonObject>, rpc::RippledError::RpcIssueMalformed),
        ifType<std::string>(kStringIssueValidator),
        ifType<JsonObject>(withCustomError(currencyIssue, rpc::RippledError::RpcIssueMalformed)),
        issueConv),
    field("amm_account", &Input::ammAccount, accountIdActMalformed),
    field("account", &Input::accountID, accountIdActMalformed));

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

}  // namespace rpc::spec::handlers::amm_info
