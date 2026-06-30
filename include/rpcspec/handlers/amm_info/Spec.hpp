/** @file */
#pragma once
// Shared constexpr spec for the 'amm_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/amm_info/Types.hpp>

#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <expected>
#include <stdexcept>
#include <string>

namespace rpc::spec::handlers::amm_info {

// Validates that a string field can be parsed as an xrpl::Issue.
// field is already confirmed to be a string (inside ifType<std::string>)
inline constexpr auto kSTRING_ISSUE_VALIDATOR =
    CustomValidator{[](auto const& f) -> MaybeError {
        try {
            xrpl::issueFromJson(std::string{f.asString()});
        } catch (std::runtime_error const&) {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
        }
        return {};
    }};

struct IssueConverter {
    static constexpr std::string_view kName = "issue";
    using ValueType = xrpl::Issue;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString()) {
            try {
                return xrpl::issueFromJson(std::string{f.asString()});
            } catch (std::runtime_error const&) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
            }
        }
        if (f.isObject()) {
            try {
                // Re-use the same issueFromJson path the old tag_invoke used.
                auto const currSv = f.child("currency").asString();
                xrpl::Currency currency{};
                if (!xrpl::toCurrency(currency, std::string{currSv}))
                    return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
                if (xrpl::isXRP(currency))
                    return xrpl::xrpIssue();
                auto const issuerSv = f.child("issuer").asString();
                xrpl::AccountID issuer{};
                if (!xrpl::toIssuer(issuer, std::string{issuerSv}))
                    return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
                return xrpl::Issue{currency, issuer};
            } catch (...) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
            }
        }
        return std::unexpected{rpc::Status{rpc::RippledError::RpcIssueMalformed}};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr auto issueConv = IssueConverter{};

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "asset",
        &Input::issue1,
        withCustomError(type<std::string, JsonObject>, rpc::RippledError::RpcIssueMalformed),
        ifType<std::string>(kSTRING_ISSUE_VALIDATOR),
        ifType<JsonObject>(withCustomError(currencyIssue, rpc::RippledError::RpcIssueMalformed)),
        issueConv
    ),
    field(
        "asset2",
        &Input::issue2,
        withCustomError(type<std::string, JsonObject>, rpc::RippledError::RpcIssueMalformed),
        ifType<std::string>(kSTRING_ISSUE_VALIDATOR),
        ifType<JsonObject>(withCustomError(currencyIssue, rpc::RippledError::RpcIssueMalformed)),
        issueConv
    ),
    field("amm_account", &Input::ammAccount, accountIdActMalformed),
    field("account", &Input::accountID, accountIdActMalformed)
);

} // namespace rpc::spec::handlers::amm_info
