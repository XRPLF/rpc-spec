/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rpc::spec::handlers::ledger_entry {

// Each locator that accepts EITHER a direct ledger-key hex OR a composite object
// is modeled as std::variant<xrpl::uint256, ...Entry>: the uint256 arm is the
// direct key, the struct arm is the unpacked object. The strong sub-field types
// mirror what the ledger_entry validator (Spec.hpp) guarantees.

/**
 * @brief `directory` object locator: an owner dir, or an explicit dir root, with an optional
 * sub-index.
 */
struct DirectoryEntry
{
    /**
     * @brief Value of the `owner` field.
     */
    std::optional<xrpl::AccountID> owner;

    /**
     * @brief Value of the `dir_root` field.
     */
    std::optional<xrpl::uint256> dirRoot;

    /**
     * @brief Value of the `sub_index` field.
     */
    std::optional<uint32_t> subIndex;
};

/**
 * @brief `offer` object locator: owner account + sequence.
 */
struct OfferEntry
{
    /**
     * @brief Value of the `account` field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `seq` field.
     */
    uint32_t seq = 0;
};

/**
 * @brief `escrow` object locator: owner account + sequence.
 */
struct EscrowEntry
{
    /**
     * @brief Value of the `owner` field.
     */
    xrpl::AccountID owner;

    /**
     * @brief Value of the `seq` field.
     */
    uint32_t seq = 0;
};

/**
 * @brief `ticket` object locator: owner account + ticket sequence.
 */
struct TicketEntry
{
    /**
     * @brief Value of the `account` field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `ticket_seq` field.
     */
    uint32_t ticketSeq = 0;
};

/**
 * @brief `permissioned_domain` object locator: owner account + sequence.
 */
struct PermissionedDomainEntry
{
    /**
     * @brief Value of the `account` field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `seq` field.
     */
    uint32_t seq = 0;
};

/**
 * @brief `vault` object locator: owner account + sequence.
 */
struct VaultEntry
{
    /**
     * @brief Value of the `owner` field.
     */
    xrpl::AccountID owner;

    /**
     * @brief Value of the `seq` field.
     */
    uint32_t seq = 0;
};

/**
 * @brief `loan_broker` object locator: owner account + sequence.
 */
struct LoanBrokerEntry
{
    /**
     * @brief Value of the `owner` field.
     */
    xrpl::AccountID owner;

    /**
     * @brief Value of the `seq` field.
     */
    uint32_t seq = 0;
};

/**
 * @brief `loan` object locator: owning loan-broker key + loan sequence.
 */
struct LoanEntry
{
    /**
     * @brief Value of the `loan_broker_id` field.
     */
    xrpl::uint256 loanBrokerId;

    /**
     * @brief Value of the `loan_seq` field.
     */
    uint32_t loanSeq = 0;
};

/**
 * @brief `delegate` object locator: account + the authorized delegate.
 */
struct DelegateEntry
{
    /**
     * @brief Value of the `account` field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `authorize` field.
     */
    xrpl::AccountID authorize;
};

/**
 * @brief `sponsorship` object locator: the sponsoring account + the sponsored account.
 */
struct SponsorshipEntry
{
    /**
     * @brief Value of the `sponsor` field.
     */
    xrpl::AccountID sponsor;

    /**
     * @brief Value of the `sponsee` field.
     */
    xrpl::AccountID sponsee;
};

/**
 * @brief `mptoken` object locator: holder account + MPT issuance id.
 */
struct MptokenEntry
{
    /**
     * @brief Value of the `account` field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `mpt_issuance_id` field.
     */
    xrpl::uint192 mptIssuanceId;
};

/**
 * @brief `amm` object locator: the two assets defining the AMM.
 */
struct AmmEntry
{
    /**
     * @brief Value of the `asset` field.
     */
    xrpl::Issue asset;

    /**
     * @brief Value of the `asset2` field.
     */
    xrpl::Issue asset2;
};

/**
 * @brief `oracle` object locator: owner account + oracle document id.
 */
struct OracleEntry
{
    /**
     * @brief Value of the `account` field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `oracle_document_id` field.
     */
    uint32_t oracleDocumentId = 0;
};

/**
 * @brief `credential` object locator: subject, issuer, and credential type.
 */
struct CredentialEntry
{
    /**
     * @brief Value of the `subject` field.
     */
    xrpl::AccountID subject;

    /**
     * @brief Value of the `issuer` field.
     */
    xrpl::AccountID issuer;

    /**
     * @brief Variable-length hex blob (credential type).
     *
     * No fixed-width strong type fits, so kept as a hex string.
     */
    std::string credentialType;
};

/**
 * @brief One entry of `deposit_preauth.authorized_credentials`.
 */
struct AuthorizeCredentialEntry
{
    /**
     * @brief Value of the `issuer` field.
     */
    xrpl::AccountID issuer;

    /**
     * @brief Variable-length hex blob (credential type).
     *
     * No fixed-width strong type fits, so kept as a hex string.
     */
    std::string credentialType;
};

/**
 * @brief `deposit_preauth` object locator: owner plus EITHER an authorized account OR a credential
 * set.
 */
struct DepositPreauthEntry
{
    /**
     * @brief Value of the `owner` field.
     */
    xrpl::AccountID owner;

    /**
     * @brief Value of the `authorized` field.
     */
    std::optional<xrpl::AccountID> authorized;

    /**
     * @brief Value of the `authorized_credentials` field.
     */
    std::optional<std::vector<AuthorizeCredentialEntry>> authorizedCredentials;
};

/**
 * @brief `ripple_state` object locator: the two trust-line accounts + currency. Object-only (no
 * hex form).
 */
struct RippleStateEntry
{
    /**
     * @brief Value of the `accounts` field.
     */
    std::array<xrpl::AccountID, 2> accounts;

    /**
     * @brief Value of the `currency` field.
     */
    xrpl::Currency currency;
};

/**
 * @brief A cross-chain bridge spec, as carried by the `bridge` and the xchain
 * claim-id locators: the two chain doors and the two chain issues.
 */
struct BridgeSpec
{
    /**
     * @brief Value of the `locking_chain_door` field.
     */
    xrpl::AccountID lockingChainDoor;

    /**
     * @brief Value of the `issuing_chain_door` field.
     */
    xrpl::AccountID issuingChainDoor;

    /**
     * @brief Value of the `locking_chain_issue` field.
     */
    xrpl::Issue lockingChainIssue;

    /**
     * @brief Value of the `issuing_chain_issue` field.
     */
    xrpl::Issue issuingChainIssue;
};

/**
 * @brief An xchain claim-id locator: the bridge spec plus the claim id.
 *
 * Used for both `xchain_owned_claim_id` and `xchain_owned_create_account_claim_id`;
 * each request field carries its own embedded bridge object and a uint32 id.
 */
struct XChainClaimIdEntry
{
    /**
     * @brief Value of the `bridge` request field.
     */
    BridgeSpec bridge;

    /**
     * @brief Value of the `claim_id` field.
     */
    uint32_t claimId = 0;
};

/**
 * @brief Input for the 'ledger_entry' RPC command.
 *
 * The wire format is unchanged from the validator; this is the strong, internal
 * representation the spec unpacks the request into. Exactly one locator member is
 * set per request (validated elsewhere); a locator's ledger-entry type is implied
 * by which member is present.
 */
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `binary` request field.
     */
    JsonBool binary{false};

    /**
     * @brief Value of the `include_deleted` request field.
     */
    JsonBool includeDeleted{false};

    /**
     * @brief Value of the `index` request field.
     */
    std::optional<xrpl::uint256> index;

    /**
     * @brief Value of the `check` request field.
     */
    std::optional<xrpl::uint256> check;

    /**
     * @brief Value of the `payment_channel` request field.
     */
    std::optional<xrpl::uint256> paymentChannel;

    /**
     * @brief Value of the `nft_page` request field.
     */
    std::optional<xrpl::uint256> nftPage;

    /**
     * @brief Value of the `nft_offer` request field.
     */
    std::optional<xrpl::uint256> nftOffer;

    /**
     * @brief Value of the `signer_list` request field.
     */
    std::optional<xrpl::uint256> signerList;

    /**
     * @brief Value of the `amendments` request field.
     */
    std::optional<xrpl::uint256> amendments;

    /**
     * @brief Value of the `fee` request field.
     */
    std::optional<xrpl::uint256> fee;

    /**
     * @brief Value of the `hashes` request field.
     */
    std::optional<xrpl::uint256> hashes;

    /**
     * @brief Value of the `nunl` request field.
     */
    std::optional<xrpl::uint256> nunl;

    /**
     * @brief Value of the `account_root` or `account` request field.
     */
    std::optional<xrpl::AccountID> accountRoot;

    /**
     * @brief Value of the `did` request field.
     */
    std::optional<xrpl::AccountID> did;

    /**
     * @brief Value of the `mpt_issuance` request field.
     */
    std::optional<xrpl::uint192> mptIssuance;

    /**
     * @brief Value of the `directory` request field.
     */
    std::optional<std::variant<xrpl::uint256, DirectoryEntry>> directory;

    /**
     * @brief Value of the `offer` request field.
     */
    std::optional<std::variant<xrpl::uint256, OfferEntry>> offer;

    /**
     * @brief Value of the `escrow` request field.
     */
    std::optional<std::variant<xrpl::uint256, EscrowEntry>> escrow;

    /**
     * @brief Value of the `deposit_preauth` request field.
     */
    std::optional<std::variant<xrpl::uint256, DepositPreauthEntry>> depositPreauth;

    /**
     * @brief Value of the `ticket` request field.
     */
    std::optional<std::variant<xrpl::uint256, TicketEntry>> ticket;

    /**
     * @brief Value of the `amm` request field.
     */
    std::optional<std::variant<xrpl::uint256, AmmEntry>> amm;

    /**
     * @brief Value of the `mptoken` request field.
     */
    std::optional<std::variant<xrpl::uint256, MptokenEntry>> mptoken;

    /**
     * @brief Value of the `permissioned_domain` request field.
     */
    std::optional<std::variant<xrpl::uint256, PermissionedDomainEntry>> permissionedDomain;

    /**
     * @brief Value of the `vault` request field.
     */
    std::optional<std::variant<xrpl::uint256, VaultEntry>> vault;

    /**
     * @brief Value of the `loan_broker` request field.
     */
    std::optional<std::variant<xrpl::uint256, LoanBrokerEntry>> loanBroker;

    /**
     * @brief Value of the `loan` request field.
     */
    std::optional<std::variant<xrpl::uint256, LoanEntry>> loan;

    /**
     * @brief Value of the `oracle` request field.
     */
    std::optional<std::variant<xrpl::uint256, OracleEntry>> oracle;

    /**
     * @brief Value of the `credential` request field.
     */
    std::optional<std::variant<xrpl::uint256, CredentialEntry>> credential;

    /**
     * @brief Value of the `delegate` request field.
     */
    std::optional<std::variant<xrpl::uint256, DelegateEntry>> delegate;

    /**
     * @brief Value of the `sponsorship` request field.
     */
    std::optional<std::variant<xrpl::uint256, SponsorshipEntry>> sponsorship;

    /**
     * @brief Value of the `ripple_state` or `state` request field.
     */
    std::optional<std::variant<xrpl::uint256, RippleStateEntry>> rippleStateAccount;

    /**
     * @brief Value of the `bridge` request field.
     */
    std::optional<BridgeSpec> bridge;

    /**
     * @brief Value of the `bridge_account` request field.
     */
    std::optional<xrpl::AccountID> bridgeAccount;

    /**
     * @brief Value of the `xchain_owned_claim_id` request field.
     */
    std::optional<std::variant<xrpl::uint256, XChainClaimIdEntry>> xchainOwnedClaimId;

    /**
     * @brief Value of the `xchain_owned_create_account_claim_id` request field.
     */
    std::optional<std::variant<xrpl::uint256, XChainClaimIdEntry>> xchainOwnedCreateAccountClaimId;
};

}  // namespace rpc::spec::handlers::ledger_entry
