/** @file */
#pragma once
// Thin xrpl wrappers extracted from rpc/RPCHelpers.hpp, util/AccountUtils.hpp,
// util/LedgerUtils.hpp, and util/TimeUtils.hpp so the spec framework has no
// outbound dependencies on those Clio headers.
// Used only by Validators.hpp.

#include <rpcspec/detail/XrplNs.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace rpc::spec::detail {

// ---- base58 / account parsing -----------------------------------------------

template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(std::string const& str)
{
    if (!std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;
    return ripple::parseBase58<T>(str);
}

template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(ripple::TokenType type, std::string const& str)
{
    if (!std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;
    return ripple::parseBase58<T>(type, str);
}

// Resolves a string that could be a hex pubkey, a base58 pubkey, or a base58
// AccountID to an AccountID.
[[nodiscard]] inline std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = ripple::strUnHex(account);
    std::optional<ripple::PublicKey> publicKey{};
    if (blob && ripple::publicKeyType(ripple::makeSlice(*blob))) {
        publicKey = ripple::PublicKey(ripple::Slice{blob->data(), blob->size()});
    } else {
        publicKey =
            parseBase58Wrapper<ripple::PublicKey>(ripple::TokenType::AccountPublic, account);
    }
    if (publicKey)
        return ripple::calcAccountID(*publicKey);
    return parseBase58Wrapper<ripple::AccountID>(account);
}

// ---- UTC date parsing -------------------------------------------------------

[[nodiscard]] inline std::optional<std::chrono::system_clock::time_point>
systemTpFromUtcStr(std::string const& dateStr, std::string const& format)
{
    std::tm ts{};
    if (strptime(dateStr.c_str(), format.c_str(), &ts) == nullptr)
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(timegm(&ts));
}

// ---- ledger entry type lookup (mirrors util::LedgerTypes) -------------------

enum class LedgerCategory { AccountOwned, Chain, DeletionBlocker };

struct LedgerTypeEntry {
    std::string_view name;
    std::string_view rpcName;
    ripple::LedgerEntryType type;
    LedgerCategory category;
};

// Single authoritative ledger type table. util::LedgerTypes (in Clio) builds on top of this.
// clang-format off
constexpr std::array<LedgerTypeEntry, 28> kLEDGER_TYPES_TABLE{{
    {.name = "AccountRoot",                     .rpcName = "account",                              .type = ripple::ltACCOUNT_ROOT,                          .category = LedgerCategory::AccountOwned},
    {.name = "Amendments",                      .rpcName = "amendments",                           .type = ripple::ltAMENDMENTS,                            .category = LedgerCategory::Chain},
    {.name = "Check",                           .rpcName = "check",                                .type = ripple::ltCHECK,                                 .category = LedgerCategory::DeletionBlocker},
    {.name = "DepositPreauth",                  .rpcName = "deposit_preauth",                      .type = ripple::ltDEPOSIT_PREAUTH,                       .category = LedgerCategory::AccountOwned},
    {.name = "DirectoryNode",                   .rpcName = "directory",                            .type = ripple::ltDIR_NODE,                              .category = LedgerCategory::Chain},
    {.name = "Escrow",                          .rpcName = "escrow",                               .type = ripple::ltESCROW,                                .category = LedgerCategory::DeletionBlocker},
    {.name = "FeeSettings",                     .rpcName = "fee",                                  .type = ripple::ltFEE_SETTINGS,                          .category = LedgerCategory::Chain},
    {.name = "LedgerHashes",                    .rpcName = "hashes",                               .type = ripple::ltLEDGER_HASHES,                         .category = LedgerCategory::Chain},
    {.name = "Offer",                           .rpcName = "offer",                                .type = ripple::ltOFFER,                                 .category = LedgerCategory::AccountOwned},
    {.name = "PayChannel",                      .rpcName = "payment_channel",                      .type = ripple::ltPAYCHAN,                               .category = LedgerCategory::DeletionBlocker},
    {.name = "SignerList",                      .rpcName = "signer_list",                          .type = ripple::ltSIGNER_LIST,                           .category = LedgerCategory::AccountOwned},
    {.name = "RippleState",                     .rpcName = "state",                                .type = ripple::ltRIPPLE_STATE,                          .category = LedgerCategory::DeletionBlocker},
    {.name = "Ticket",                          .rpcName = "ticket",                               .type = ripple::ltTICKET,                                .category = LedgerCategory::AccountOwned},
    {.name = "NFTokenOffer",                    .rpcName = "nft_offer",                            .type = ripple::ltNFTOKEN_OFFER,                         .category = LedgerCategory::AccountOwned},
    {.name = "NFTokenPage",                     .rpcName = "nft_page",                             .type = ripple::ltNFTOKEN_PAGE,                          .category = LedgerCategory::DeletionBlocker},
    {.name = "AMM",                             .rpcName = "amm",                                  .type = ripple::ltAMM,                                   .category = LedgerCategory::AccountOwned},
    {.name = "Bridge",                          .rpcName = "bridge",                               .type = ripple::ltBRIDGE,                                .category = LedgerCategory::DeletionBlocker},
    {.name = "XChainOwnedClaimID",              .rpcName = "xchain_owned_claim_id",                .type = ripple::ltXCHAIN_OWNED_CLAIM_ID,                 .category = LedgerCategory::DeletionBlocker},
    {.name = "XChainOwnedCreateAccountClaimID", .rpcName = "xchain_owned_create_account_claim_id", .type = ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID, .category = LedgerCategory::DeletionBlocker},
    {.name = "DID",                             .rpcName = "did",                                  .type = ripple::ltDID,                                   .category = LedgerCategory::AccountOwned},
    {.name = "Oracle",                          .rpcName = "oracle",                               .type = ripple::ltORACLE,                                .category = LedgerCategory::AccountOwned},
    {.name = "Credential",                      .rpcName = "credential",                           .type = ripple::ltCREDENTIAL,                            .category = LedgerCategory::AccountOwned},
    {.name = "Vault",                           .rpcName = "vault",                                .type = ripple::ltVAULT,                                 .category = LedgerCategory::AccountOwned},
    {.name = "NegativeUNL",                     .rpcName = "nunl",                                 .type = ripple::ltNEGATIVE_UNL,                          .category = LedgerCategory::Chain},
    {.name = "MPTokenIssuance",                 .rpcName = "mpt_issuance",                         .type = ripple::ltMPTOKEN_ISSUANCE,                      .category = LedgerCategory::DeletionBlocker},
    {.name = "MPToken",                         .rpcName = "mptoken",                              .type = ripple::ltMPTOKEN,                               .category = LedgerCategory::DeletionBlocker},
    {.name = "PermissionedDomain",              .rpcName = "permissioned_domain",                  .type = ripple::ltPERMISSIONED_DOMAIN,                   .category = LedgerCategory::DeletionBlocker},
    {.name = "Delegate",                        .rpcName = "delegate",                             .type = ripple::ltDELEGATE,                              .category = LedgerCategory::AccountOwned},
}};
// clang-format on

struct LedgerTypeInfo {
    ripple::LedgerEntryType type;
    LedgerCategory category;
};

[[nodiscard]] inline std::optional<LedgerTypeInfo>
ledgerTypeInfoFromStr(std::string const& entryName)
{
    // Exact rpc-name match (e.g. "account", "nft_offer").
    static auto const kRPC_MAP = []() {
        std::unordered_map<std::string, LedgerTypeInfo> m;
        for (auto const& e : kLEDGER_TYPES_TABLE)
            m.emplace(std::string{e.rpcName}, LedgerTypeInfo{.type = e.type, .category = e.category});
        return m;
    }();
    // Case-insensitive canonical-name match (e.g. "AccountRoot" → "accountroot").
    static auto const kNAME_MAP = []() {
        std::unordered_map<std::string, LedgerTypeInfo> m;
        for (auto const& e : kLEDGER_TYPES_TABLE) {
            std::string lower{e.name};
            std::ranges::transform(
                lower, lower.begin(), [](unsigned char c) { return std::tolower(c); }
            );
            m.emplace(std::move(lower), LedgerTypeInfo{.type = e.type, .category = e.category});
        }
        return m;
    }();

    if (auto it = kRPC_MAP.find(entryName); it != kRPC_MAP.end())
        return it->second;

    std::string lower{entryName};
    std::ranges::transform(
        lower, lower.begin(), [](unsigned char c) { return std::tolower(c); }
    );
    if (auto it = kNAME_MAP.find(lower); it != kNAME_MAP.end())
        return it->second;

    return std::nullopt;
}

[[nodiscard]] inline ripple::LedgerEntryType
ledgerEntryTypeFromStr(std::string const& name)
{
    auto const info = ledgerTypeInfoFromStr(name);
    return info ? info->type : ripple::ltANY;
}

[[nodiscard]] inline ripple::LedgerEntryType
accountOwnedLedgerTypeFromStr(std::string const& name)
{
    auto const info = ledgerTypeInfoFromStr(name);
    if (info && info->category != LedgerCategory::Chain)
        return info->type;
    return ripple::ltANY;
}

}  // namespace rpc::spec::detail
