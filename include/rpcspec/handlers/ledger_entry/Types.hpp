/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

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

/** @brief `directory` object locator: an owner dir, or an explicit dir root, with an optional sub-index. */
struct DirectoryEntry {
    std::optional<xrpl::AccountID> owner;
    std::optional<xrpl::uint256> dirRoot;
    std::optional<uint32_t> subIndex;
};

/** @brief `offer` object locator: owner account + sequence. */
struct OfferEntry {
    xrpl::AccountID account;
    uint32_t seq = 0;
};

/** @brief `escrow` object locator: owner account + sequence. */
struct EscrowEntry {
    xrpl::AccountID owner;
    uint32_t seq = 0;
};

/** @brief `ticket` object locator: owner account + ticket sequence. */
struct TicketEntry {
    xrpl::AccountID account;
    uint32_t ticketSeq = 0;
};

/** @brief `permissioned_domain` object locator: owner account + sequence. */
struct PermissionedDomainEntry {
    xrpl::AccountID account;
    uint32_t seq = 0;
};

/** @brief `vault` object locator: owner account + sequence. */
struct VaultEntry {
    xrpl::AccountID owner;
    uint32_t seq = 0;
};

/** @brief `loan_broker` object locator: owner account + sequence. */
struct LoanBrokerEntry {
    xrpl::AccountID owner;
    uint32_t seq = 0;
};

/** @brief `loan` object locator: owning loan-broker key + loan sequence. */
struct LoanEntry {
    xrpl::uint256 loanBrokerId;
    uint32_t loanSeq = 0;
};

/** @brief `delegate` object locator: account + the authorized delegate. */
struct DelegateEntry {
    xrpl::AccountID account;
    xrpl::AccountID authorize;
};

/** @brief `mptoken` object locator: holder account + MPT issuance id. */
struct MptokenEntry {
    xrpl::AccountID account;
    xrpl::uint192 mptIssuanceId;
};

/** @brief `amm` object locator: the two assets defining the AMM. */
struct AmmEntry {
    xrpl::Issue asset;
    xrpl::Issue asset2;
};

/** @brief `oracle` object locator: owner account + oracle document id. */
struct OracleEntry {
    xrpl::AccountID account;
    uint32_t oracleDocumentId = 0;
};

/** @brief `credential` object locator: subject, issuer, and credential type. */
struct CredentialEntry {
    xrpl::AccountID subject;
    xrpl::AccountID issuer;
    std::string credentialType;  /**< Variable-length hex blob (credential type); no fixed-width strong type fits, so kept as a hex string. */
};

/** @brief One entry of `deposit_preauth.authorized_credentials`. */
struct AuthorizeCredentialEntry {
    xrpl::AccountID issuer;
    std::string credentialType;  /**< Variable-length hex blob (credential type); no fixed-width strong type fits, so kept as a hex string. */
};

/** @brief `deposit_preauth` object locator: owner plus EITHER an authorized account OR a credential set. */
struct DepositPreauthEntry {
    xrpl::AccountID owner;
    std::optional<xrpl::AccountID> authorized;
    std::optional<std::vector<AuthorizeCredentialEntry>> authorizedCredentials;
};

/** @brief `ripple_state` object locator: the two trust-line accounts + currency. Object-only (no hex form). */
struct RippleStateEntry {
    std::array<xrpl::AccountID, 2> accounts;
    xrpl::Currency currency;
};

/**
 * @brief A cross-chain bridge spec, as carried by the `bridge` and the xchain
 * claim-id locators: the two chain doors and the two chain issues.
 */
struct BridgeSpec {
    xrpl::AccountID lockingChainDoor;
    xrpl::AccountID issuingChainDoor;
    xrpl::Issue lockingChainIssue;
    xrpl::Issue issuingChainIssue;
};

/**
 * @brief An xchain claim-id locator: the bridge spec plus the claim id.
 *
 * Used for both `xchain_owned_claim_id` and `xchain_owned_create_account_claim_id`;
 * each request field carries its own embedded bridge object and a uint32 id.
 */
struct XChainClaimIdEntry {
    BridgeSpec bridge;
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
struct Input {
    LedgerSpecifier ledger;
    JsonBool binary{false};
    JsonBool includeDeleted{false};

    // Direct ledger-key locators (hex string -> xrpl::uint256).
    std::optional<xrpl::uint256> index;
    std::optional<xrpl::uint256> check;
    std::optional<xrpl::uint256> paymentChannel;
    std::optional<xrpl::uint256> nftPage;
    std::optional<xrpl::uint256> nftOffer;
    std::optional<xrpl::uint256> signerList;
    std::optional<xrpl::uint256> amendments;
    std::optional<xrpl::uint256> fee;
    std::optional<xrpl::uint256> hashes;
    std::optional<xrpl::uint256> nunl;

    // Account / id locators.
    std::optional<xrpl::AccountID> accountRoot;
    std::optional<xrpl::AccountID> did;
    std::optional<xrpl::uint192> mptIssuance;

    // Hex-or-object locators.
    std::optional<std::variant<xrpl::uint256, DirectoryEntry>> directory;
    std::optional<std::variant<xrpl::uint256, OfferEntry>> offer;
    std::optional<std::variant<xrpl::uint256, EscrowEntry>> escrow;
    std::optional<std::variant<xrpl::uint256, DepositPreauthEntry>> depositPreauth;
    std::optional<std::variant<xrpl::uint256, TicketEntry>> ticket;
    std::optional<std::variant<xrpl::uint256, AmmEntry>> amm;
    std::optional<std::variant<xrpl::uint256, MptokenEntry>> mptoken;
    std::optional<std::variant<xrpl::uint256, PermissionedDomainEntry>> permissionedDomain;
    std::optional<std::variant<xrpl::uint256, VaultEntry>> vault;
    std::optional<std::variant<xrpl::uint256, LoanBrokerEntry>> loanBroker;
    std::optional<std::variant<xrpl::uint256, LoanEntry>> loan;
    std::optional<std::variant<xrpl::uint256, OracleEntry>> oracle;
    std::optional<std::variant<xrpl::uint256, CredentialEntry>> credential;
    std::optional<std::variant<xrpl::uint256, DelegateEntry>> delegate;

    // Object-only locators.
    std::optional<RippleStateEntry> rippleStateAccount;
    std::optional<BridgeSpec> bridge;
    std::optional<xrpl::AccountID> bridgeAccount;
    std::optional<std::variant<xrpl::uint256, XChainClaimIdEntry>> xchainOwnedClaimId;
    std::optional<std::variant<xrpl::uint256, XChainClaimIdEntry>> xchainOwnedCreateAccountClaimId;
};

} // namespace rpc::spec::handlers::ledger_entry
