/** @file */
#pragma once

#include <xrpl/protocol/LedgerFormats.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rpc::spec {

/**
 * @brief How a ledger object relates to an account: owned, chain-wide, or a deletion blocker.
 */
enum class LedgerCategory { AccountOwned, Chain, DeletionBlocker };

/**
 * @brief One row of the ledger type registry.
 */
struct LedgerTypeEntry
{
    std::string_view name;
    std::string_view rpcName;
    xrpl::LedgerEntryType type;
    LedgerCategory category;
};

/**
 * @brief Every known ledger object type, with its RPC name and category.
 */
// clang-format off
constexpr std::array<LedgerTypeEntry, 30> kLedgerTypesTable{{
    {.name = "AccountRoot",                     .rpcName = "account",                              .type = xrpl::ltACCOUNT_ROOT,                          .category = LedgerCategory::AccountOwned},
    {.name = "Amendments",                      .rpcName = "amendments",                           .type = xrpl::ltAMENDMENTS,                            .category = LedgerCategory::Chain},
    {.name = "Check",                           .rpcName = "check",                                .type = xrpl::ltCHECK,                                 .category = LedgerCategory::DeletionBlocker},
    {.name = "DepositPreauth",                  .rpcName = "deposit_preauth",                      .type = xrpl::ltDEPOSIT_PREAUTH,                       .category = LedgerCategory::AccountOwned},
    {.name = "DirectoryNode",                   .rpcName = "directory",                            .type = xrpl::ltDIR_NODE,                              .category = LedgerCategory::Chain},
    {.name = "Escrow",                          .rpcName = "escrow",                               .type = xrpl::ltESCROW,                                .category = LedgerCategory::DeletionBlocker},
    {.name = "FeeSettings",                     .rpcName = "fee",                                  .type = xrpl::ltFEE_SETTINGS,                          .category = LedgerCategory::Chain},
    {.name = "LedgerHashes",                    .rpcName = "hashes",                               .type = xrpl::ltLEDGER_HASHES,                         .category = LedgerCategory::Chain},
    {.name = "Offer",                           .rpcName = "offer",                                .type = xrpl::ltOFFER,                                 .category = LedgerCategory::AccountOwned},
    {.name = "PayChannel",                      .rpcName = "payment_channel",                      .type = xrpl::ltPAYCHAN,                               .category = LedgerCategory::DeletionBlocker},
    {.name = "SignerList",                      .rpcName = "signer_list",                          .type = xrpl::ltSIGNER_LIST,                           .category = LedgerCategory::AccountOwned},
    {.name = "RippleState",                     .rpcName = "state",                                .type = xrpl::ltRIPPLE_STATE,                          .category = LedgerCategory::DeletionBlocker},
    {.name = "Ticket",                          .rpcName = "ticket",                               .type = xrpl::ltTICKET,                                .category = LedgerCategory::AccountOwned},
    {.name = "NFTokenOffer",                    .rpcName = "nft_offer",                            .type = xrpl::ltNFTOKEN_OFFER,                         .category = LedgerCategory::AccountOwned},
    {.name = "NFTokenPage",                     .rpcName = "nft_page",                             .type = xrpl::ltNFTOKEN_PAGE,                          .category = LedgerCategory::DeletionBlocker},
    {.name = "AMM",                             .rpcName = "amm",                                  .type = xrpl::ltAMM,                                   .category = LedgerCategory::AccountOwned},
    {.name = "Bridge",                          .rpcName = "bridge",                               .type = xrpl::ltBRIDGE,                                .category = LedgerCategory::DeletionBlocker},
    {.name = "XChainOwnedClaimID",              .rpcName = "xchain_owned_claim_id",                .type = xrpl::ltXCHAIN_OWNED_CLAIM_ID,                 .category = LedgerCategory::DeletionBlocker},
    {.name = "XChainOwnedCreateAccountClaimID", .rpcName = "xchain_owned_create_account_claim_id", .type = xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID, .category = LedgerCategory::DeletionBlocker},
    {.name = "DID",                             .rpcName = "did",                                  .type = xrpl::ltDID,                                   .category = LedgerCategory::AccountOwned},
    {.name = "Oracle",                          .rpcName = "oracle",                               .type = xrpl::ltORACLE,                                .category = LedgerCategory::AccountOwned},
    {.name = "Credential",                      .rpcName = "credential",                           .type = xrpl::ltCREDENTIAL,                            .category = LedgerCategory::AccountOwned},
    {.name = "Vault",                           .rpcName = "vault",                                .type = xrpl::ltVAULT,                                 .category = LedgerCategory::AccountOwned},
    // loan broker is a pseudo-account object, like AMM and Vault
    {.name = "LoanBroker",                      .rpcName = "loan_broker",                          .type = xrpl::ltLOAN_BROKER,                           .category = LedgerCategory::AccountOwned},
    {.name = "Loan",                            .rpcName = "loan",                                 .type = xrpl::ltLOAN,                                  .category = LedgerCategory::DeletionBlocker},
    {.name = "NegativeUNL",                     .rpcName = "nunl",                                 .type = xrpl::ltNEGATIVE_UNL,                          .category = LedgerCategory::Chain},
    {.name = "MPTokenIssuance",                 .rpcName = "mpt_issuance",                         .type = xrpl::ltMPTOKEN_ISSUANCE,                      .category = LedgerCategory::DeletionBlocker},
    {.name = "MPToken",                         .rpcName = "mptoken",                              .type = xrpl::ltMPTOKEN,                               .category = LedgerCategory::DeletionBlocker},
    {.name = "PermissionedDomain",              .rpcName = "permissioned_domain",                  .type = xrpl::ltPERMISSIONED_DOMAIN,                   .category = LedgerCategory::DeletionBlocker},
    {.name = "Delegate",                        .rpcName = "delegate",                             .type = xrpl::ltDELEGATE,                              .category = LedgerCategory::AccountOwned},
}};
// clang-format on

/**
 * @brief The type and category of a ledger object, resolved from its name.
 */
struct LedgerTypeInfo
{
    xrpl::LedgerEntryType type;
    LedgerCategory category;
};

/**
 * @brief Resolve a ledger type by RPC name or by canonical name (case-insensitive).
 *
 * @param entryName The RPC name (e.g. "nft_offer") or canonical name (e.g. "NFTokenOffer")
 * @return The type and category, or nullopt when the name is unknown
 */
[[nodiscard]] inline std::optional<LedgerTypeInfo>
ledgerTypeInfoFromStr(std::string const& entryName)
{
    // Exact rpc-name match (e.g. "account", "nft_offer").
    static auto const kRpcMap = [] {
        std::unordered_map<std::string, LedgerTypeInfo> byRpcName;
        for (auto const& entry : kLedgerTypesTable)
        {
            byRpcName.emplace(
                std::string{entry.rpcName},
                LedgerTypeInfo{.type = entry.type, .category = entry.category});
        }
        return byRpcName;
    }();
    // Case-insensitive canonical-name match (e.g. "AccountRoot" → "accountroot").
    static auto const kNameMap = [] {
        std::unordered_map<std::string, LedgerTypeInfo> byCanonicalName;
        for (auto const& entry : kLedgerTypesTable)
        {
            std::string lower{entry.name};
            std::ranges::transform(
                lower, lower.begin(), [](unsigned char chr) { return std::tolower(chr); });
            byCanonicalName.emplace(
                std::move(lower), LedgerTypeInfo{.type = entry.type, .category = entry.category});
        }
        return byCanonicalName;
    }();

    if (auto it = kRpcMap.find(entryName); it != kRpcMap.end())
        return it->second;

    std::string lower{entryName};
    std::ranges::transform(
        lower, lower.begin(), [](unsigned char chr) { return std::tolower(chr); });
    if (auto it = kNameMap.find(lower); it != kNameMap.end())
        return it->second;

    return std::nullopt;
}

/**
 * @brief Resolve any ledger object type from its name.
 *
 * @param name The RPC or canonical name
 * @return The matching type, or xrpl::ltANY when unknown
 */
[[nodiscard]] inline xrpl::LedgerEntryType
ledgerEntryTypeFromStr(std::string const& name)
{
    auto const info = ledgerTypeInfoFromStr(name);
    return info ? info->type : xrpl::ltANY;
}

/**
 * @brief Resolve an account-owned ledger object type from its name.
 *
 * @param name The RPC or canonical name
 * @return The matching type, or xrpl::ltANY when unknown or chain-wide
 */
[[nodiscard]] inline xrpl::LedgerEntryType
accountOwnedLedgerTypeFromStr(std::string const& name)
{
    auto const info = ledgerTypeInfoFromStr(name);
    if (info && info->category != LedgerCategory::Chain)
        return info->type;
    return xrpl::ltANY;
}

}  // namespace rpc::spec
