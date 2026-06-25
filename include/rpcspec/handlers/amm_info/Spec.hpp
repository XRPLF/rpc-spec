/** @file */
#pragma once
// Shared constexpr spec for the 'amm_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/handlers/amm_info/Types.hpp>

#include <xrpl/protocol/Issue.h>

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

inline constexpr auto kSpec = RpcSpec{
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field(
        "asset",
        withCustomError(type<std::string, JsonObject>, rpc::RippledError::RpcIssueMalformed),
        ifType<std::string>(kSTRING_ISSUE_VALIDATOR),
        ifType<JsonObject>(withCustomError(currencyIssue, rpc::RippledError::RpcIssueMalformed))
    ),
    field(
        "asset2",
        withCustomError(type<std::string, JsonObject>, rpc::RippledError::RpcIssueMalformed),
        ifType<std::string>(kSTRING_ISSUE_VALIDATOR),
        ifType<JsonObject>(withCustomError(currencyIssue, rpc::RippledError::RpcIssueMalformed))
    ),
    field("amm_account", withCustomError(account, rpc::RippledError::RpcActMalformed)),
    field("account", withCustomError(account, rpc::RippledError::RpcActMalformed)),
};

} // namespace rpc::spec::handlers::amm_info
