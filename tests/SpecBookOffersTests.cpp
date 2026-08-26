/** @file
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

#include <string>

using namespace rpc::spec;

namespace {

constexpr char const* kMPT_ID = "000004C463C52827307480341125DA0577DEFC38405DBADD";

auto
parseBookOffers(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::book_offers::kInputSpec.parse(value);
}

}  // namespace

TEST(BookOffersSpec, CurrencyOnlyTakerParses)
{
    auto const r = parseBookOffers(R"JSON({
        "taker_gets": {"currency": "XRP"},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    EXPECT_TRUE(r->takerGets.holds<xrpl::Issue>());
    EXPECT_TRUE(r->takerPays.holds<xrpl::Issue>());
}

TEST(BookOffersSpec, MptIssuanceIdParsesAsMptIssue)
{
    auto const r = parseBookOffers(
        std::string{R"JSON({
        "taker_gets": {"mpt_issuance_id": ")JSON"} +
        kMPT_ID + R"JSON("},
        "taker_pays": {"mpt_issuance_id": ")JSON" +
        kMPT_ID + R"JSON("}
    })JSON");
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;

    ASSERT_TRUE(r->takerGets.holds<xrpl::MPTIssue>());
    ASSERT_TRUE(r->takerPays.holds<xrpl::MPTIssue>());

    xrpl::MPTID expected{};
    ASSERT_TRUE(expected.parseHex(kMPT_ID));
    EXPECT_EQ(r->takerGets.get<xrpl::MPTIssue>().getMptID(), expected);
    EXPECT_EQ(r->takerPays.get<xrpl::MPTIssue>().getMptID(), expected);
}

TEST(BookOffersSpec, MptIssuanceIdWithCurrencyFails)
{
    auto const r = parseBookOffers(
        std::string{R"JSON({
        "taker_gets": {"currency": "USD", "mpt_issuance_id": ")JSON"} +
        kMPT_ID + R"JSON("},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Invalid field 'taker_gets'.");
}

TEST(BookOffersSpec, MptIssuanceIdWithIssuerFails)
{
    auto const r = parseBookOffers(
        std::string{R"JSON({
        "taker_gets": {"currency": "XRP"},
        "taker_pays": {"mpt_issuance_id": ")JSON"} +
        kMPT_ID + R"JSON(", "issuer": "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"}
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Invalid field 'taker_pays'.");
}

TEST(BookOffersSpec, NeitherCurrencyNorMptIssuanceIdFails)
{
    auto const r = parseBookOffers(R"JSON({
        "taker_gets": {},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Missing field 'taker_gets.currency'.");
}

TEST(BookOffersSpec, NonStringCurrencyIsInvalidParams)
{
    auto const r = parseBookOffers(R"JSON({
        "taker_gets": {"currency": 123},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Invalid field 'taker_gets.currency', not string.");
}

TEST(BookOffersSpec, MalformedTakerGetsMptIdIsDstAmtMalformed)
{
    auto const r = parseBookOffers(R"JSON({
        "taker_gets": {"mpt_issuance_id": "NOTAHEX"},
        "taker_pays": {"currency": "XRP"}
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcDstAmtMalformed);
}

TEST(BookOffersSpec, MalformedTakerPaysMptIdIsSrcCurMalformed)
{
    auto const r = parseBookOffers(R"JSON({
        "taker_gets": {"currency": "XRP"},
        "taker_pays": {"mpt_issuance_id": "NOTAHEX"}
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcSrcCurMalformed);
}
