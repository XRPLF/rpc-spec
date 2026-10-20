/**
 * @file
 * @brief Clio-backend arms of the handler specs that branch on RPCSPEC_IS_CLIO.
 *
 * `vault_info`, `ledger_data`, `ledger` and `transaction_entry` are the handler
 * specs whose field errors differ per server. Compiled with RPCSPEC_IS_CLIO=1 (see
 * rpcspec_clio_tests), this translation unit is the only place those branches run;
 * the xrpld wording is pinned by SpecVaultInfoTests / SpecLedgerDataTests /
 * SpecTransactionEntryTests.
 *
 * Keeping both sides asserted is deliberate — vault_info's error contract has
 * already drifted between the two servers once.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/AssetTypes.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_channels/Spec.hpp>
#include <rpcspec/handlers/account_channels/Types.hpp>
#include <rpcspec/handlers/account_currencies/Spec.hpp>
#include <rpcspec/handlers/account_currencies/Types.hpp>
#include <rpcspec/handlers/account_info/Spec.hpp>
#include <rpcspec/handlers/account_lines/Spec.hpp>
#include <rpcspec/handlers/account_lines/Types.hpp>
#include <rpcspec/handlers/account_nfts/Spec.hpp>
#include <rpcspec/handlers/account_nfts/Types.hpp>
#include <rpcspec/handlers/account_offers/Spec.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>
#include <rpcspec/handlers/amm_info/Spec.hpp>
#include <rpcspec/handlers/amm_info/Types.hpp>
#include <rpcspec/handlers/book_offers/Spec.hpp>
#include <rpcspec/handlers/book_offers/Types.hpp>
#include <rpcspec/handlers/deposit_authorized/Spec.hpp>
#include <rpcspec/handlers/deposit_authorized/Types.hpp>
#include <rpcspec/handlers/get_aggregate_price/Spec.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>
#include <rpcspec/handlers/ledger/Spec.hpp>
#include <rpcspec/handlers/ledger_data/Spec.hpp>
#include <rpcspec/handlers/ledger_data/Types.hpp>
#include <rpcspec/handlers/nft_offers_common/Spec.hpp>
#include <rpcspec/handlers/nft_offers_common/Types.hpp>
#include <rpcspec/handlers/transaction_entry/Spec.hpp>
#include <rpcspec/handlers/vault_info/Spec.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <format>
#include <string>
#include <variant>

using namespace rpc::spec;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kMptId = "000004C463C52827307480341125DA0577DEFC38405DBADD";
constexpr auto kPublicKeyHex = "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020";

auto
parseBookOffers(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::book_offers::kInputSpec.parse(value);
}

auto
parseAmmInfo(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::amm_info::kInputSpec.parse(value);
}

auto
parseAggregatePrice(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::get_aggregate_price::kInputSpec.parse(value);
}

auto
parseVault(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::vault_info::kInputSpec.parse(value);
}

auto
parseLedgerData(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::ledger_data::kInputSpec.parse(value);
}

auto
parseTransactionEntry(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::transaction_entry::kInputSpec.parse(value);
}

auto
parseAccountCurrencies(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::account_currencies::kInputSpec.parse(value);
}

auto
parseAccountNfts(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::account_nfts::kInputSpec.parse(value);
}

auto
parseAccountOffers(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::account_offers::kInputSpecV1.parse(value);
}

auto
parseAccountLines(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::account_lines::kInputSpecV1.parse(value);
}

auto
parseAccountChannels(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::account_channels::kInputSpec.parse(value);
}

auto
parseDepositAuthorized(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::deposit_authorized::kInputSpec.parse(value);
}

auto
parseNftOffers(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::nft_offers_common::kInputSpec.parse(value);
}

auto
parseAccountInfo(std::string const& json, unsigned apiVersion = 1)
{
    auto value = boost::json::parse(json);
    return apiVersion == 1 ? handlers::account_info::kInputSpecV1.parse(value)
                           : handlers::account_info::kInputSpecV2.parse(value);
}

}  // namespace

// --- vault_info: every field error collapses onto RpcMalformedRequest -------

TEST(VaultInfoSpecClio, valid_request_still_parses)
{
    auto const result = parseVault(std::format(R"JSON({{"owner": "{}", "seq": 5}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(VaultInfoSpecClio, non_hex_vault_id_is_bare_malformed_request)
{
    auto const result = parseVault(R"JSON({"vault_id": "NOTHEX"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::ClioError::RpcMalformedRequest);
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}

TEST(VaultInfoSpecClio, malformed_owner_carries_owner_not_hex_string)
{
    auto const result = parseVault(R"JSON({"owner": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::ClioError::RpcMalformedRequest);
    EXPECT_EQ(result.error().message, "OwnerNotHexString");
}

TEST(VaultInfoSpecClio, zero_account_owner_is_rejected_on_clio_only)
{
    // accountBase58 additionally rejects the all-zero AccountID; xrpld's
    // parseVault() accepts it, so this arm is Clio-only by construction.
    auto const result = parseVault(R"JSON({"owner": "rrrrrrrrrrrrrrrrrrrrrhoLvTp"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::ClioError::RpcMalformedRequest);
}

TEST(VaultInfoSpecClio, non_integer_seq_is_bare_malformed_request)
{
    auto const result = parseVault(R"JSON({"seq": "5"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::ClioError::RpcMalformedRequest);
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}

TEST(VaultInfoSpecClio, seq_zero_is_accepted_by_spec)
{
    // Matches the xrpld arm: the spec type-checks only.
    auto const result = parseVault(std::format(R"JSON({{"owner": "{}", "seq": 0}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

// --- ledger_data: the marker type failure is message-less on Clio -----------

TEST(LedgerDataSpecClio, hex_string_marker_still_parses)
{
    auto const result = parseLedgerData(std::format(R"JSON({{"marker": "{}"}})JSON", kHex1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_TRUE(std::holds_alternative<xrpl::uint256>(*result->marker));
}

TEST(LedgerDataSpecClio, other_marker_types_are_message_less)
{
    // xrpld emits "markerNotString" here; Clio deliberately emits nothing.
    for (auto const* bad : {"true", "{}", "[]", "-1"})
    {
        auto const result = parseLedgerData(std::format(R"JSON({{"marker": {}}})JSON", bad));
        ASSERT_FALSE(result.has_value()) << "marker=" << bad << " unexpectedly accepted";
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_TRUE(result.error().message.empty())
            << "marker=" << bad << " unexpected: " << result.error().message;
    }
}

TEST(LedgerDataSpecClio, non_hex_string_marker_names_the_field)
{
    // This arm is shared: malformedFieldMessage() gives "markerMalformed" on Clio.
    auto const result = parseLedgerData(R"JSON({"marker": "NOTHEX"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerMalformed");
}

TEST(LedgerSpecClio, ledger_is_validated_before_other_fields)
{
    for (auto const version : {1u, 2u})
    {
        auto value = boost::json::parse(R"JSON({ "ledger_index": "potato", "full": "blah" })JSON");
        auto const result = handlers::ledger::kSpec.parse(value, version);
        ASSERT_FALSE(result.has_value()) << version;
        EXPECT_EQ(result.error().message, "ledgerIndexMalformed") << version;
    }
}

TEST(LedgerSpecClio, diff_must_be_bool)
{
    for (auto const version : {1u, 2u})
    {
        auto value = boost::json::parse(R"JSON({ "diff": "yes" })JSON");
        auto const result = handlers::ledger::kSpec.parse(value, version);
        ASSERT_FALSE(result.has_value()) << version;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << version;
    }
}

// --- transaction_entry: tx_hash is rejected by the spec on Clio only --------

TEST(TransactionEntrySpecClio, valid_hash_parses)
{
    auto const result = parseTransactionEntry(std::format(R"JSON({{"tx_hash": "{}"}})JSON", kHex1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->txHash.has_value());

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHex1));
    EXPECT_EQ(*result->txHash, expected);
}

TEST(TransactionEntrySpecClio, missing_hash_is_field_not_found_transaction)
{
    auto const result = parseTransactionEntry(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::ClioError::RpcFieldNotFoundTransaction);
}

TEST(TransactionEntrySpecClio, non_hex_hash_is_malformed)
{
    auto const result = parseTransactionEntry(R"JSON({"tx_hash": "DEADBEEF"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "tx_hashMalformed");
}

TEST(TransactionEntrySpecClio, non_string_hash_is_not_string)
{
    auto const result = parseTransactionEntry(R"JSON({"tx_hash": 42})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "tx_hashNotString");
}

TEST(AccountCurrenciesSpecClio, valid_account_parses)
{
    auto const result =
        parseAccountCurrencies(std::format(R"JSON({{"account": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpecClio, missing_account_is_required)
{
    auto const result = parseAccountCurrencies(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Required field 'account' missing");
}

TEST(AccountCurrenciesSpecClio, ident_is_not_a_fallback)
{
    auto const result = parseAccountCurrencies(std::format(R"JSON({{"ident": "{}"}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Required field 'account' missing");
}

TEST(AccountCurrenciesSpecClio, malformed_ident_is_ignored_beside_account)
{
    auto const result = parseAccountCurrencies(
        std::format(R"JSON({{"account": "{}", "ident": "notanaccount"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpecClio, malformed_account_is_rejected)
{
    auto const result = parseAccountCurrencies(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "accountMalformed");
}

TEST(AccountCurrenciesSpecClio, limit_is_clamped)
{
    auto const result =
        parseAccountCurrencies(std::format(R"JSON({{"account": "{}", "limit": 1}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, handlers::account_currencies::kLimitMin);
}

TEST(AccountCurrenciesSpecClio, malformed_marker_is_malformed_cursor)
{
    auto const notString =
        parseAccountCurrencies(std::format(R"JSON({{"account": "{}", "marker": 5}})JSON", kAcct1));
    ASSERT_FALSE(notString.has_value());
    EXPECT_EQ(notString.error().message, "markerNotString");

    auto const malformed = parseAccountCurrencies(
        std::format(R"JSON({{"account": "{}", "marker": "{},7,x"}})JSON", kAcct1, kHex1));
    ASSERT_FALSE(malformed.has_value());
    EXPECT_EQ(malformed.error().message, "Malformed cursor.");
}

TEST(AccountCurrenciesSpecClio, deprecated_fields_warn)
{
    auto const request = boost::json::parse(
        std::format(R"JSON({{"account": "{}", "account_index": 1, "strict": true}})JSON", kAcct1));
    EXPECT_EQ(handlers::account_currencies::kInputSpec.check(request).size(), 2u);
}

TEST(AccountNftsSpecClio, limit_is_clamped)
{
    auto const result =
        parseAccountNfts(std::format(R"JSON({{"account": "{}", "limit": 1}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, handlers::account_nfts::kLimitMin);
}

TEST(AccountNftsSpecClio, limit_is_checked_before_marker)
{
    auto const result = parseAccountNfts(
        std::format(R"JSON({{"account": "{}", "limit": "x", "marker": 5}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(AccountOffersSpecClio, malformed_account_is_rejected)
{
    auto const result = parseAccountOffers(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "accountMalformed");
}

TEST(AccountOffersSpecClio, limit_is_clamped)
{
    auto const result =
        parseAccountOffers(std::format(R"JSON({{"account": "{}", "limit": 1}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, handlers::account_offers::kLimitMin);
}

TEST(AccountOffersSpecClio, non_string_marker_is_not_string)
{
    auto const result =
        parseAccountOffers(std::format(R"JSON({{"account": "{}", "marker": 5}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "markerNotString");
}

TEST(AccountOffersSpecClio, marker_text_after_a_second_comma_is_rejected)
{
    auto const result = parseAccountOffers(
        std::format(R"JSON({{"account": "{}", "marker": "{},7,x"}})JSON", kAcct1, kHex1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Malformed cursor.");
}

TEST(AccountOffersSpecClio, deprecated_fields_warn)
{
    auto const request = boost::json::parse(
        std::format(R"JSON({{"account": "{}", "ledger": 5, "strict": true}})JSON", kAcct1));
    EXPECT_EQ(handlers::account_offers::kInputSpecV1.check(request).size(), 2u);
}

TEST(AccountLinesSpecClio, non_string_account_is_act_malformed)
{
    auto const result = parseAccountLines(R"JSON({"account": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
}

TEST(AccountLinesSpecClio, empty_peer_is_act_malformed)
{
    auto const result =
        parseAccountLines(std::format(R"JSON({{"account": "{}", "peer": ""}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
}

TEST(AccountLinesSpecClio, peer_is_missing_when_absent)
{
    auto const absent = parseAccountLines(std::format(R"JSON({{"account": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(absent.has_value())
        << "error: " << absent.error().error << " msg: " << absent.error().message;
    ASSERT_FALSE(absent->peer.has_value());
    EXPECT_EQ(absent->peer.error(), DeferredError::Missing);

    auto const present = parseAccountLines(
        std::format(R"JSON({{"account": "{}", "peer": "{}"}})JSON", kAcct1, kAcct2));
    ASSERT_TRUE(present.has_value())
        << "error: " << present.error().error << " msg: " << present.error().message;
    EXPECT_TRUE(present->peer.has_value());
}

TEST(AccountLinesSpecClio, ignore_default_must_be_bool)
{
    auto const result = parseAccountLines(
        std::format(R"JSON({{"account": "{}", "ignore_default": 1}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(AccountLinesSpecClio, limit_is_clamped)
{
    auto const result =
        parseAccountLines(std::format(R"JSON({{"account": "{}", "limit": 1}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, handlers::account_lines::kLimitMin);
}

TEST(AccountLinesSpecClio, malformed_marker_is_malformed_cursor)
{
    auto const notString =
        parseAccountLines(std::format(R"JSON({{"account": "{}", "marker": 5}})JSON", kAcct1));
    ASSERT_FALSE(notString.has_value());
    EXPECT_EQ(notString.error().message, "markerNotString");

    auto const malformed = parseAccountLines(
        std::format(R"JSON({{"account": "{}", "marker": "{},7,x"}})JSON", kAcct1, kHex1));
    ASSERT_FALSE(malformed.has_value());
    EXPECT_EQ(malformed.error().message, "Malformed cursor.");
}

TEST(AccountLinesSpecClio, deprecated_fields_warn)
{
    auto const request = boost::json::parse(
        std::format(R"JSON({{"account": "{}", "ledger": 5, "peer_index": 3}})JSON", kAcct1));
    EXPECT_EQ(handlers::account_lines::kInputSpecV1.check(request).size(), 2u);
}

TEST(AccountChannelsSpecClio, non_string_destination_account_is_invalid_params)
{
    auto const result = parseAccountChannels(
        std::format(R"JSON({{"account": "{}", "destination_account": 5}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(AccountChannelsSpecClio, empty_destination_account_is_malformed)
{
    auto const result = parseAccountChannels(
        std::format(R"JSON({{"account": "{}", "destination_account": ""}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "destination_accountMalformed");
}

TEST(AccountChannelsSpecClio, limit_is_clamped)
{
    auto const result =
        parseAccountChannels(std::format(R"JSON({{"account": "{}", "limit": 1}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, handlers::account_channels::kLimitMin);
}

TEST(AccountInfoSpecClio, neither_leaves_account_missing)
{
    auto const result = parseAccountInfo(R"JSON({})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_FALSE(result->account.has_value());
    EXPECT_EQ(result->account.error(), DeferredError::Missing);
}

TEST(AccountInfoSpecClio, ident_is_read_when_account_is_absent)
{
    auto const result = parseAccountInfo(std::format(R"JSON({{"ident": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountInfoSpecClio, account_wins_over_ident)
{
    auto const result = parseAccountInfo(
        std::format(R"JSON({{"account": "{}", "ident": "{}"}})JSON", kAcct1, kAcct2));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    auto const accountOnly =
        parseAccountInfo(std::format(R"JSON({{"account": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(accountOnly.has_value());
    EXPECT_EQ(result->account, accountOnly->account);
}

TEST(AccountInfoSpecClio, malformed_ident_is_rejected_beside_account)
{
    auto const result = parseAccountInfo(
        std::format(R"JSON({{"account": "{}", "ident": "notanaccount"}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "identMalformed");
}

TEST(AccountInfoSpecClio, ident_is_parsed_without_account)
{
    auto const result = parseAccountInfo(R"JSON({"ident": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "identMalformed");
}

TEST(AccountInfoSpecClio, signer_lists_must_be_bool_on_v2)
{
    auto const request =
        std::format(R"JSON({{"account": "{}", "signer_lists": "yes"}})JSON", kAcct1);
    EXPECT_TRUE(parseAccountInfo(request, 1).has_value());

    auto const v2 = parseAccountInfo(request, 2);
    ASSERT_FALSE(v2.has_value());
    EXPECT_EQ(v2.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(AccountInfoSpecClio, deprecated_fields_warn)
{
    auto const request = boost::json::parse(
        std::format(R"JSON({{"ident": "{}", "ledger": 5, "strict": true}})JSON", kAcct1));
    EXPECT_EQ(handlers::account_info::kInputSpecV1.check(request).size(), 3u);
}

TEST(AccountInfoSpecClio, ident_warns_beside_account)
{
    auto const request = boost::json::parse(
        std::format(R"JSON({{"account": "{}", "ident": "{}"}})JSON", kAcct1, kAcct1));
    EXPECT_EQ(handlers::account_info::kInputSpecV1.check(request).size(), 1u);
}

TEST(DepositAuthorizedSpecClio, non_string_account_is_not_string)
{
    auto const result = parseDepositAuthorized(
        std::format(R"JSON({{"source_account": 5, "destination_account": "{}"}})JSON", kAcct2));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "source_accountNotString");
}

TEST(DepositAuthorizedSpecClio, empty_credentials_parse)
{
    auto const result = parseDepositAuthorized(
        std::format(
            R"JSON({{"source_account": "{}", "destination_account": "{}", "credentials": []}})JSON",
            kAcct1,
            kAcct2));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->credentials.has_value());
    EXPECT_TRUE(result->credentials->empty());
}

TEST(DepositAuthorizedSpecClio, non_array_credentials_is_bare_invalid_params)
{
    auto const result = parseDepositAuthorized(
        std::format(
            R"JSON({{"source_account": "{}", "destination_account": "{}", "credentials": "x"}})JSON",
            kAcct1,
            kAcct2));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}

TEST(DepositAuthorizedSpecClio, non_hex_credential_is_item_error)
{
    auto const result = parseDepositAuthorized(
        std::format(
            R"JSON({{"source_account": "{}", "destination_account": "{}", "credentials": ["x"]}})JSON",
            kAcct1,
            kAcct2));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Item is not a valid uint256 type.");
}

TEST(NftOffersSpecClio, limit_is_clamped)
{
    auto const result =
        parseNftOffers(std::format(R"JSON({{"nft_id": "{}", "limit": 1}})JSON", kHex1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, handlers::nft_offers_common::kLimitMin);
}

TEST(NftOffersSpecClio, non_string_marker_is_not_string)
{
    auto const result =
        parseNftOffers(std::format(R"JSON({{"nft_id": "{}", "marker": 5}})JSON", kHex1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerNotString");
}

TEST(NftOffersSpecClio, non_hex_marker_is_malformed)
{
    auto const result =
        parseNftOffers(std::format(R"JSON({{"nft_id": "{}", "marker": "x"}})JSON", kHex1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerMalformed");
}

TEST(BookOffersSpecClio, non_string_mpt_issuance_id_names_mpt_issuance_id_field)
{
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {"mpt_issuance_id": 123},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'taker_gets.mpt_issuance_id', not string.");
}

TEST(BookOffersSpecClio, non_object_taker_is_invalid_params)
{
    auto const result =
        parseBookOffers(R"JSON({"taker_pays": null, "taker_gets": {"currency": "XRP"}})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(BookOffersSpecClio, issued_currency_without_issuer_is_left_to_the_handler)
{
    auto const result = parseBookOffers(
        R"JSON({"taker_pays": {"currency": "USD"}, "taker_gets": {"currency": "XRP"}})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->takerPays.holds<xrpl::Issue>());
    EXPECT_EQ(result->takerPays.get<xrpl::Issue>().account, xrpl::xrpAccount());
}

TEST(BookOffersSpecClio, taker_may_be_a_public_key)
{
    auto const result = parseBookOffers(
        std::format(
            R"JSON({{"taker_pays": {{"currency": "XRP"}}, "taker_gets": {{"currency": "XRP"}}, "taker": "{}"}})JSON",
            kPublicKeyHex));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->taker.has_value());
}

TEST(BookOffersSpecClio, limit_is_clamped)
{
    auto const result = parseBookOffers(
        R"JSON({"taker_pays": {"currency": "XRP"}, "taker_gets": {"currency": "XRP"}, "limit": 1000})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, handlers::book_offers::kLimitMax);
}

TEST(AmmInfoSpecClio, assets_and_accounts_parse)
{
    auto const result = parseAmmInfo(
        std::format(
            R"JSON({{"asset": {{"currency": "USD", "issuer": "{}"}}, "asset2": {{"currency": "XRP"}}, "account": "{}", "amm_account": "{}"}})JSON",
            kAcct1,
            kPublicKeyHex,
            kAcct2));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->asset1.has_value());
    EXPECT_TRUE(result->asset1->holds<xrpl::Issue>());
    EXPECT_EQ(result->asset2, DeferredAsset{xrpl::xrpIssue()});
    EXPECT_TRUE(result->accountID.has_value());
    EXPECT_TRUE(result->ammAccount.has_value());
}

TEST(AmmInfoSpecClio, absent_fields_are_missing)
{
    auto const result = parseAmmInfo(R"JSON({})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->asset1, DeferredAsset{std::unexpected{DeferredError::Missing}});
    EXPECT_EQ(result->accountID, DeferredAccountId{std::unexpected{DeferredError::Missing}});
}

TEST(AmmInfoSpecClio, xrp_asset_yields_xrp_issue)
{
    auto const result = parseAmmInfo(R"JSON({"asset": {"currency": "XRP"}})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->asset1, DeferredAsset{xrpl::xrpIssue()});
}

TEST(AmmInfoSpecClio, bad_asset_is_issue_malformed)
{
    for (auto const& asset : {
             std::string{"123"},
             std::string{"true"},
             std::string{"[]"},
             std::string{"null"},
             std::string{R"JSON({"currency": "TOOLONGCURRENCY"})JSON"},
             std::string{R"JSON({"currency": "USD", "issuer": "notanaccount"})JSON"},
             std::format(R"JSON({{"issuer": "{}"}})JSON", kAcct1),
             std::string{R"JSON({"currency": "USD"})JSON"},
             std::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId),
         })
    {
        for (auto const* key : {"asset", "asset2"})
        {
            auto const result = parseAmmInfo(std::format(R"JSON({{"{}": {}}})JSON", key, asset));
            ASSERT_FALSE(result.has_value()) << key << "=" << asset;
            EXPECT_EQ(result.error(), rpc::XrpldError::RpcIssueMalformed) << key << "=" << asset;
        }
    }
}

TEST(AmmInfoSpecClio, bad_account_is_act_malformed)
{
    for (auto const* account : {R"("notanaccount")", "42"})
    {
        for (auto const* key : {"account", "amm_account"})
        {
            auto const result = parseAmmInfo(std::format(R"JSON({{"{}": {}}})JSON", key, account));
            ASSERT_FALSE(result.has_value()) << key << "=" << account;
            EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed) << key << "=" << account;
        }
    }
}

TEST(GetAggregatePriceSpecClio, oracles_parse)
{
    auto const result = parseAggregatePrice(
        std::format(
            R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "oracles": [{{"oracle_document_id": "7", "account": "{}"}}]}})JSON",
            kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->oracles.has_value());
    EXPECT_EQ((*result->oracles)[0].documentId, 7u);
}

TEST(GetAggregatePriceSpecClio, required_fields)
{
    for (
        auto const& json : {
            std::format(
                R"JSON({{"quote_asset": "XRP", "oracles": [{{"oracle_document_id": 1, "account": "{}"}}]}})JSON",
                kAcct1),
            std::string{R"JSON({"base_asset": "USD", "quote_asset": "XRP"})JSON"},
        })
    {
        auto const result = parseAggregatePrice(json);
        ASSERT_FALSE(result.has_value()) << json;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << json;
    }
}

TEST(GetAggregatePriceSpecClio, bad_oracles_fail_the_parse)
{
    auto const withOracles = [](std::string const& oracles) {
        return std::format(
            R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "oracles": {}}})JSON", oracles);
    };
    auto const withElement = [&](std::string const& docId, std::string const& account) {
        return withOracles(
            std::format(
                R"JSON([{{"oracle_document_id": {}, "account": {}}}])JSON", docId, account));
    };
    auto const account = std::format(R"JSON("{}")JSON", kAcct1);

    for (auto const& json : {
             withOracles(R"JSON("nope")JSON"),
             withOracles("[]"),
             withOracles("[1]"),
             withOracles(std::format(R"JSON([{{"account": "{}"}}])JSON", kAcct1)),
             withOracles(R"JSON([{"oracle_document_id": 1}])JSON"),
         })
    {
        auto const result = parseAggregatePrice(json);
        ASSERT_FALSE(result.has_value()) << json;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcOracleMalformed) << json;
    }

    for (auto const& json : {
             withElement("-1", account),
             withElement("null", account),
             withElement("2.3", account),
             withElement("true", account),
             withElement("4294967296", account),
             withElement(R"JSON("a")JSON", account),
             withElement("1", R"JSON("notanaccount")JSON"),
             withElement("1", "5"),
         })
    {
        auto const result = parseAggregatePrice(json);
        ASSERT_FALSE(result.has_value()) << json;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << json;
    }
}

TEST(GetAggregatePriceSpecClio, trim_and_time_threshold)
{
    auto const withField = [](std::string const& key, std::string const& value) {
        return std::format(
            R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "{}": {}, "oracles": [{{"oracle_document_id": 1, "account": "{}"}}]}})JSON",
            key,
            value,
            kAcct1);
    };

    for (auto const& json : {
             withField("trim", "1"),
             withField("trim", "25"),
             withField("time_threshold", "60"),
         })
    {
        auto const result = parseAggregatePrice(json);
        ASSERT_TRUE(result.has_value()) << json << " msg: " << result.error().message;
    }

    for (auto const& json : {
             withField("trim", "0"),
             withField("trim", "26"),
             withField("trim", R"JSON("5")JSON"),
             withField("time_threshold", R"JSON("60")JSON"),
         })
    {
        auto const result = parseAggregatePrice(json);
        ASSERT_FALSE(result.has_value()) << json;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << json;
    }
}
