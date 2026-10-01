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
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/ledger/Spec.hpp>
#include <rpcspec/handlers/ledger_data/Spec.hpp>
#include <rpcspec/handlers/ledger_data/Types.hpp>
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
constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

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
