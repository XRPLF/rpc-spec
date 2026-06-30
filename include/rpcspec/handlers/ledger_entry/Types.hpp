/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/STXChainBridge.h>
#include <xrpl/protocol/UintTypes.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rpc::spec::handlers::ledger_entry {

// Each locator below that accepts EITHER a direct ledger-key hex OR a composite
// object is modeled as std::variant<xrpl::uint256, ...Entry>: the uint256 arm is
// the direct key, the struct arm is the unpacked object. The strong sub-field
// types mirror what the ledger_entry validator (Spec.hpp) guarantees.

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

/** @brief One entry of `deposit_preauth.authorized_credentials`. */
struct AuthorizeCredentialEntry {
    xrpl::AccountID issuer;
    std::string credentialType;  /**< Variable-length hex blob (credential type). */
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
 * @brief Input for the 'ledger_entry' RPC command.
 *
 * @note This handler is still a validate-only spec (Spec.hpp): the spec checks the
 * request shape, while the values below are unpacked from the request downstream.
 * The types are the strong contract that unpacking must produce.
 */
struct Input {
    std::optional<xrpl::uint256> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    bool binary = false;
    // Direct ledger-entry key. Also the normalized target for the hex-only
    // locators (check, payment_channel, nft_page, nft_offer, signer_list,
    // amendments, fee, hashes, nunl), with expectedType recording which.
    std::optional<xrpl::uint256> index;
    xrpl::LedgerEntryType expectedType = xrpl::ltANY;
    std::optional<xrpl::AccountID> accountRoot;
    std::optional<xrpl::AccountID> did;
    std::optional<xrpl::uint192> mptIssuance;
    std::optional<std::variant<xrpl::uint256, DirectoryEntry>> directory;
    std::optional<std::variant<xrpl::uint256, OfferEntry>> offer;
    std::optional<RippleStateEntry> rippleStateAccount;
    std::optional<std::variant<xrpl::uint256, EscrowEntry>> escrow;
    std::optional<std::variant<xrpl::uint256, DepositPreauthEntry>> depositPreauth;
    std::optional<std::variant<xrpl::uint256, TicketEntry>> ticket;
    std::optional<std::variant<xrpl::uint256, AmmEntry>> amm;
    std::optional<std::variant<xrpl::uint256, MptokenEntry>> mptoken;
    std::optional<std::variant<xrpl::uint256, PermissionedDomainEntry>> permissionedDomain;
    std::optional<std::variant<xrpl::uint256, VaultEntry>> vault;
    std::optional<std::variant<xrpl::uint256, LoanBrokerEntry>> loanBroker;
    std::optional<std::variant<xrpl::uint256, LoanEntry>> loan;
    std::optional<xrpl::STXChainBridge> bridge;
    std::optional<xrpl::AccountID> bridgeAccount;
    std::optional<uint32_t> chainClaimId;
    std::optional<uint32_t> createAccountClaimId;
    std::optional<xrpl::uint256> oracleNode;
    std::optional<xrpl::uint256> credential;
    std::optional<std::variant<xrpl::uint256, DelegateEntry>> delegate;
    bool includeDeleted = false;
};

} // namespace rpc::spec::handlers::ledger_entry
