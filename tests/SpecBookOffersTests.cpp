/**
 * @file
 *  GTest coverage for the `book_offers` typed spec, focused on the taker asset rules
 *  (MPTokensV2): a taker object names its asset with either `currency` (optionally plus
 *  `issuer`) or `mpt_issuance_id`, never both.
 *
 *  The expected codes and messages mirror Clio's BookOffersTests wire contract.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/book_offers/Spec.hpp>
#include <rpcspec/handlers/book_offers/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <format>
#include <string>

using namespace rpc::spec;

namespace {

constexpr auto kMptId = "000004C463C52827307480341125DA0577DEFC38405DBADD";

auto
parseBookOffers(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::book_offers::kInputSpec.parse(value);
}

}  // namespace

TEST(BookOffersSpec, CurrencyOnlyTakerParses)
{
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {"currency": "XRP"},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_TRUE(result->takerGets.holds<xrpl::Issue>());
    EXPECT_TRUE(result->takerPays.holds<xrpl::Issue>());
}

TEST(BookOffersSpec, MptIssuanceIdParsesAsMptIssue)
{
    auto const result = parseBookOffers(
        std::format(
            R"JSON({{
        "taker_gets": {{"mpt_issuance_id": "{}"}},
        "taker_pays": {{"mpt_issuance_id": "{}"}}
    }})JSON",
            kMptId,
            kMptId));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;

    ASSERT_TRUE(result->takerGets.holds<xrpl::MPTIssue>());
    ASSERT_TRUE(result->takerPays.holds<xrpl::MPTIssue>());

    xrpl::MPTID expected{};
    ASSERT_TRUE(expected.parseHex(kMptId));
    EXPECT_EQ(result->takerGets.get<xrpl::MPTIssue>().getMptID(), expected);
    EXPECT_EQ(result->takerPays.get<xrpl::MPTIssue>().getMptID(), expected);
}

TEST(BookOffersSpec, MptIssuanceIdWithCurrencyFails)
{
    auto const result = parseBookOffers(
        std::format(
            R"JSON({{
        "taker_gets": {{"currency": "USD", "mpt_issuance_id": "{}"}},
        "taker_pays": {{"currency": "XRP"}}
    }})JSON",
            kMptId));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'taker_gets'.");
}

TEST(BookOffersSpec, MptIssuanceIdWithIssuerFails)
{
    auto const result = parseBookOffers(
        std::format(
            R"JSON({{
        "taker_gets": {{"currency": "XRP"}},
        "taker_pays": {{"mpt_issuance_id": "{}", "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"}}
    }})JSON",
            kMptId));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'taker_pays'.");
}

TEST(BookOffersSpec, NeitherCurrencyNorMptIssuanceIdFails)
{
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'taker_gets.currency'.");
}

TEST(BookOffersSpec, NonStringCurrencyIsInvalidParams)
{
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {"currency": 123},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'taker_gets.currency', not string.");
}

TEST(BookOffersSpec, MalformedTakerGetsMptIdIsDstAmtMalformed)
{
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {"mpt_issuance_id": "NOTAHEX"},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcDstAmtMalformed);
}

TEST(BookOffersSpec, MalformedTakerPaysMptIdIsSrcCurMalformed)
{
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {"currency": "XRP"},
        "taker_pays": {"mpt_issuance_id": "NOTAHEX"}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcSrcCurMalformed);
}

TEST(BookOffersSpec, NonStringMptIssuanceIdNamesMptIssuanceIdField)
{
    // Deliberately diverges from xrpld, which names `.currency` here — see #3205.
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {"mpt_issuance_id": 123},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'taker_gets.mpt_issuance_id', not string.");
}

TEST(BookOffersSpec, NonStringTakerPaysMptIssuanceIdNamesMptIssuanceIdField)
{
    auto const result = parseBookOffers(R"JSON({
        "taker_gets": {"currency": "XRP"},
        "taker_pays": {"mpt_issuance_id": true}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'taker_pays.mpt_issuance_id', not string.");
}
