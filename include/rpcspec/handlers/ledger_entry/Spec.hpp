/** @file */
#pragma once
// Shared constexpr spec for the 'ledger_entry' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>

#include <xrpl/protocol/AccountID.h>

namespace rpc::spec::handlers::ledger_entry {

// Validator only works in this handler
// The accounts array must have two different elements
// Each element must be a valid address
inline constexpr auto kRIPPLE_STATE_ACCOUNTS_VALIDATOR =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isArray() || f.arraySize() != 2) {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "malformedAccounts"}
            };
        }
        auto const elem0 = f.element(0);
        auto const elem1 = f.element(1);
        if (!elem0.isString() || !elem1.isString() || elem0.asString() == elem1.asString()) {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "malformedAccounts"}
            };
        }
        auto const id1 =
            rpc::spec::detail::parseBase58Wrapper<xrpl::AccountID>(std::string{elem0.asString()});
        auto const id2 =
            rpc::spec::detail::parseBase58Wrapper<xrpl::AccountID>(std::string{elem1.asString()});
        if (!id1 || !id2) {
            return std::unexpected{
                rpc::Status{rpc::ClioError::RpcMalformedAddress, "malformedAddresses"}
            };
        }
        return {};
    }};

inline constexpr auto kMALFORMED_REQUEST_HEX_STRING_VALIDATOR =
    withCustomError(uint256Hex, rpc::ClioError::RpcMalformedRequest);

inline constexpr auto kMALFORMED_REQUEST_INT_VALIDATOR =
    withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest);

inline constexpr auto kBRIDGE_JSON_VALIDATOR = withCustomError(
    ifType<JsonObject>(section(
        field("LockingChainDoor", required, accountBase58),
        field("IssuingChainDoor", required, accountBase58),
        field("LockingChainIssue", required, currencyIssue),
        field("IssuingChainIssue", required, currencyIssue)
    )),
    rpc::ClioError::RpcMalformedRequest
);

inline constexpr auto kSpec = RpcSpec{
    field("binary", type<bool>),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("index", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field("account_root", accountBase58),
    field("did", accountBase58),
    field("check", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field(
        "deposit_preauth",
        type<std::string, JsonObject>,
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "owner",
                required,
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)
            ),
            field("authorized", accountBase58),
            field("authorized_credentials", authorizeCredential)
        ))
    ),
    field(
        "directory",
        type<std::string, JsonObject>,
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field("owner", accountBase58),
            field("dir_root", uint256Hex),
            field("sub_index", kMALFORMED_REQUEST_INT_VALIDATOR)
        ))
    ),
    field(
        "escrow",
        type<std::string, JsonObject>,
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "owner",
                required,
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)
            ),
            field("seq", required, kMALFORMED_REQUEST_INT_VALIDATOR)
        ))
    ),
    field(
        "offer",
        type<std::string, JsonObject>,
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field("account", required, accountBase58),
            field("seq", required, kMALFORMED_REQUEST_INT_VALIDATOR)
        ))
    ),
    field("payment_channel", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field(
        "ripple_state",
        type<JsonObject>,
        section(
            field("accounts", required, kRIPPLE_STATE_ACCOUNTS_VALIDATOR),
            field("currency", required, currency)
        )
    ),
    field(
        "ticket",
        type<std::string, JsonObject>,
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field("account", required, accountBase58),
            field("ticket_seq", required, kMALFORMED_REQUEST_INT_VALIDATOR)
        ))
    ),
    field("nft_page", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field(
        "amm",
        type<std::string, JsonObject>,
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "asset",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<JsonObject>, rpc::ClioError::RpcMalformedRequest),
                currencyIssue
            ),
            field(
                "asset2",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<JsonObject>, rpc::ClioError::RpcMalformedRequest),
                currencyIssue
            )
        ))
    ),
    field(
        "bridge",
        withCustomError(type<JsonObject>, rpc::ClioError::RpcMalformedRequest),
        kBRIDGE_JSON_VALIDATOR
    ),
    field(
        "bridge_account", withCustomError(accountBase58, rpc::ClioError::RpcMalformedRequest)
    ),
    field(
        "xchain_owned_claim_id",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        kBRIDGE_JSON_VALIDATOR,
        withCustomError(
            ifType<JsonObject>(section(field("xchain_owned_claim_id", required, type<uint32_t>))),
            rpc::ClioError::RpcMalformedRequest
        )
    ),
    field(
        "xchain_owned_create_account_claim_id",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        kBRIDGE_JSON_VALIDATOR,
        withCustomError(
            ifType<JsonObject>(section(
                field("xchain_owned_create_account_claim_id", required, type<uint32_t>)
            )),
            rpc::ClioError::RpcMalformedRequest
        )
    ),
    field(
        "oracle",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(withCustomError(
            kMALFORMED_REQUEST_HEX_STRING_VALIDATOR, rpc::ClioError::RpcMalformedAddress
        )),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)
            ),
            // note: Unlike `rippled`, Clio only supports UInt as input, no string, no
            // `null`, etc.:
            field(
                "oracle_document_id",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(
                    type<uint32_t, std::string>, rpc::ClioError::RpcMalformedOracleDocumentId
                ),
                withCustomError(toNumber, rpc::ClioError::RpcMalformedOracleDocumentId)
            )
        ))
    ),
    field(
        "credential",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(withCustomError(
            kMALFORMED_REQUEST_HEX_STRING_VALIDATOR, rpc::ClioError::RpcMalformedAddress
        )),
        ifType<JsonObject>(section(
            field(
                "subject",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)
            ),
            field(
                "issuer",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)
            ),
            field(
                "credential_type",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<std::string>, rpc::ClioError::RpcMalformedRequest)
            )
        ))
    ),
    field("mpt_issuance", withCustomError(uint192Hex, rpc::ClioError::RpcMalformedRequest)),
    field(
        "mptoken",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)
            ),
            field(
                "mpt_issuance_id",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(uint192Hex, rpc::ClioError::RpcMalformedRequest)
            )
        ))
    ),
    field(
        "permissioned_domain",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)
            ),
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)
            )
        ))
    ),
    field(
        "vault",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)
            ),
            field(
                "owner",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)
            )
        ))
    ),
    field(
        "loan_broker",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)
            ),
            field(
                "owner",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)
            )
        ))
    ),
    field(
        "loan",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "loan_seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)
            ),
            field(
                "loan_broker_id",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(uint256Hex, rpc::ClioError::RpcMalformedRequest)
            )
        ))
    ),
    field(
        "delegate",
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)
            ),
            field(
                "authorize",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)
            )
        ))
    ),
    field("amendments", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field("fee", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field("hashes", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field("nft_offer", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field("nunl", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field("signer_list", kMALFORMED_REQUEST_HEX_STRING_VALIDATOR),
    field("ledger", deprecated),
    field("include_deleted", type<bool>),
};

} // namespace rpc::spec::handlers::ledger_entry
