/** @file */
#pragma once

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/tokens.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace rpc::spec::detail {

template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(std::string const& str)
{
    if (!std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;
    return xrpl::parseBase58<T>(str);
}

template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(xrpl::TokenType type, std::string const& str)
{
    if (!std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;
    return xrpl::parseBase58<T>(type, str);
}

[[nodiscard]] inline std::optional<xrpl::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = xrpl::strUnHex(account);
    std::optional<xrpl::PublicKey> publicKey{};
    if (blob && xrpl::publicKeyType(xrpl::makeSlice(*blob)))
    {
        publicKey = xrpl::PublicKey(xrpl::Slice{blob->data(), blob->size()});
    }
    else
    {
        publicKey = parseBase58Wrapper<xrpl::PublicKey>(xrpl::TokenType::AccountPublic, account);
    }
    if (publicKey)
        return xrpl::calcAccountID(*publicKey);
    return parseBase58Wrapper<xrpl::AccountID>(account);
}

// "Validated extraction" helpers: convert a value the spec validator already
// confirmed into its strong type. They check the precondition (and throw if it
// is somehow violated) before dereferencing, so the conversion is total and
// clang-tidy can see the access is guarded. Use these in converters that run
// after a validator has guaranteed the input shape.

/** @brief Decode a validated account string into an xrpl::AccountID. */
[[nodiscard]] inline xrpl::AccountID
accountFromValidated(std::string const& account)
{
    auto const id = accountFromStringStrict(account);
    if (!id)
        throw std::logic_error("accountFromValidated: account was not pre-validated");
    return *id;
}

/** @brief Decode a validated hex string into an xrpl::uint256. */
[[nodiscard]] inline xrpl::uint256
uint256FromValidated(std::string const& hex)
{
    xrpl::uint256 out;
    if (!out.parseHex(hex.c_str()))
        throw std::logic_error("uint256FromValidated: hex was not pre-validated");
    return out;
}

/** @brief Decode a validated hex string into an xrpl::uint192. */
[[nodiscard]] inline xrpl::uint192
uint192FromValidated(std::string const& hex)
{
    xrpl::uint192 out;
    if (!out.parseHex(hex.c_str()))
        throw std::logic_error("uint192FromValidated: hex was not pre-validated");
    return out;
}

/** @brief Decode a validated currency-code string into an xrpl::Currency. */
[[nodiscard]] inline xrpl::Currency
currencyFromValidated(std::string const& code)
{
    xrpl::Currency out;
    if (!xrpl::toCurrency(out, code))
        throw std::logic_error("currencyFromValidated: currency was not pre-validated");
    return out;
}

/** @brief Decode a validated issuer string into an xrpl::AccountID. */
[[nodiscard]] inline xrpl::AccountID
issuerFromValidated(std::string const& issuer)
{
    xrpl::AccountID out;
    if (!xrpl::toIssuer(out, issuer))
        throw std::logic_error("issuerFromValidated: issuer was not pre-validated");
    return out;
}

[[nodiscard]] inline std::optional<std::chrono::system_clock::time_point>
systemTpFromUtcStr(std::string const& dateStr, std::string const& format)
{
    std::tm ts{};
    if (strptime(dateStr.c_str(), format.c_str(), &ts) == nullptr)
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(timegm(&ts));
}

enum class LedgerCategory { AccountOwned, Chain, DeletionBlocker };

struct LedgerTypeEntry
{
    std::string_view name;
    std::string_view rpcName;
    xrpl::LedgerEntryType type;
    LedgerCategory category;
};

// clang-format off
constexpr std::array<LedgerTypeEntry, 28> kLedgerTypesTable{{
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
    {.name = "NegativeUNL",                     .rpcName = "nunl",                                 .type = xrpl::ltNEGATIVE_UNL,                          .category = LedgerCategory::Chain},
    {.name = "MPTokenIssuance",                 .rpcName = "mpt_issuance",                         .type = xrpl::ltMPTOKEN_ISSUANCE,                      .category = LedgerCategory::DeletionBlocker},
    {.name = "MPToken",                         .rpcName = "mptoken",                              .type = xrpl::ltMPTOKEN,                               .category = LedgerCategory::DeletionBlocker},
    {.name = "PermissionedDomain",              .rpcName = "permissioned_domain",                  .type = xrpl::ltPERMISSIONED_DOMAIN,                   .category = LedgerCategory::DeletionBlocker},
    {.name = "Delegate",                        .rpcName = "delegate",                             .type = xrpl::ltDELEGATE,                              .category = LedgerCategory::AccountOwned},
}};
// clang-format on

struct LedgerTypeInfo
{
    xrpl::LedgerEntryType type;
    LedgerCategory category;
};

[[nodiscard]] inline std::optional<LedgerTypeInfo>
ledgerTypeInfoFromStr(std::string const& entryName)
{
    // Exact rpc-name match (e.g. "account", "nft_offer").
    static auto const kRpcMap = []() {
        std::unordered_map<std::string, LedgerTypeInfo> m;
        for (auto const& e : kLedgerTypesTable)
        {
            m.emplace(
                std::string{e.rpcName}, LedgerTypeInfo{.type = e.type, .category = e.category});
        }
        return m;
    }();
    // Case-insensitive canonical-name match (e.g. "AccountRoot" → "accountroot").
    static auto const kNameMap = []() {
        std::unordered_map<std::string, LedgerTypeInfo> m;
        for (auto const& e : kLedgerTypesTable)
        {
            std::string lower{e.name};
            std::ranges::transform(
                lower, lower.begin(), [](unsigned char c) { return std::tolower(c); });
            m.emplace(std::move(lower), LedgerTypeInfo{.type = e.type, .category = e.category});
        }
        return m;
    }();

    if (auto it = kRpcMap.find(entryName); it != kRpcMap.end())
        return it->second;

    std::string lower{entryName};
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return std::tolower(c); });
    if (auto it = kNameMap.find(lower); it != kNameMap.end())
        return it->second;

    return std::nullopt;
}

[[nodiscard]] inline xrpl::LedgerEntryType
ledgerEntryTypeFromStr(std::string const& name)
{
    auto const info = ledgerTypeInfoFromStr(name);
    return info ? info->type : xrpl::ltANY;
}

[[nodiscard]] inline xrpl::LedgerEntryType
accountOwnedLedgerTypeFromStr(std::string const& name)
{
    auto const info = ledgerTypeInfoFromStr(name);
    if (info && info->category != LedgerCategory::Chain)
        return info->type;
    return xrpl::ltANY;
}

}  // namespace rpc::spec::detail
