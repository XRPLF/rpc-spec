/**
 * @file
 *  GTest coverage for the `book_offers` typed spec, focused on the taker asset rules
 *  (MPTokensV2): a taker object names its asset with either `currency` (optionally plus
 *  `issuer`) or `mpt_issuance_id`, never both.
 *  Compiled under RPCSPEC_IS_XRPLD, so the expected codes and messages are xrpld's. The Clio arm
 *  is pinned in SpecClioHandlerErrorsTests.
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
#include <utility>

using namespace rpc::spec;

namespace {

constexpr auto kMptId = "000004C463C52827307480341125DA0577DEFC38405DBADD";
constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAccountZero = "rrrrrrrrrrrrrrrrrrrrrhoLvTp";
constexpr auto kAccountOne = "rrrrrrrrrrrrrrrrrrrrBZbvji";
constexpr auto kPublicKeyHex = "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020";
constexpr auto kXrp = R"JSON({"currency": "XRP"})JSON";

auto
parseBookOffers(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::book_offers::kInputSpec.parse(value);
}

std::string
request(std::string const& pays, std::string const& gets, std::string const& extra = {})
{
    return std::format(R"JSON({{"taker_pays": {}, "taker_gets": {}{}}})JSON", pays, gets, extra);
}

void
expectError(std::string const& json, rpc::XrpldError code, std::string const& message)
{
    auto const result = parseBookOffers(json);
    ASSERT_FALSE(result.has_value()) << json << " unexpectedly accepted";
    EXPECT_EQ(result.error(), code) << json;
    EXPECT_EQ(result.error().message, message) << json;
}

// Asserts @p takerJson is rejected the same way on either side, with that side's code and name.
void
expectTakerError(
    std::string const& takerJson,
    rpc::XrpldError paysCode,
    rpc::XrpldError getsCode,
    std::string const& messageFormat)
{
    expectError(
        request(takerJson, kXrp),
        paysCode,
        std::vformat(messageFormat, std::make_format_args("taker_pays")));
    expectError(
        request(kXrp, takerJson),
        getsCode,
        std::vformat(messageFormat, std::make_format_args("taker_gets")));
}

}  // namespace

TEST(BookOffersSpec, currency_only_taker_parses)
{
    auto const result = parseBookOffers(request(kXrp, kXrp));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_TRUE(result->takerGets.holds<xrpl::Issue>());
    EXPECT_TRUE(result->takerPays.holds<xrpl::Issue>());
}

TEST(BookOffersSpec, empty_currency_is_xrp)
{
    auto const result = parseBookOffers(request(R"JSON({"currency": ""})JSON", kXrp));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_EQ(result->takerPays, xrpl::Asset{xrpl::xrpIssue()});
}

TEST(BookOffersSpec, xrp_with_the_xrp_account_as_issuer_parses)
{
    auto const result = parseBookOffers(request(
        std::format(R"JSON({{"currency": "XRP", "issuer": "{}"}})JSON", kAccountZero), kXrp));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_EQ(result->takerPays, xrpl::Asset{xrpl::xrpIssue()});
}

TEST(BookOffersSpec, issued_currency_parses)
{
    auto const result = parseBookOffers(
        request(std::format(R"JSON({{"currency": "USD", "issuer": "{}"}})JSON", kAcct1), kXrp));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->takerPays.holds<xrpl::Issue>());
    EXPECT_FALSE(xrpl::isXRP(result->takerPays.get<xrpl::Issue>().currency));
}

TEST(BookOffersSpec, hex_issuer_parses)
{
    auto const result = parseBookOffers(request(
        R"JSON({"currency": "USD", "issuer": "B5F762798A53D543A014CAF8B297CFF8F2F937E8"})JSON",
        kXrp));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->takerPays.holds<xrpl::Issue>());
    EXPECT_FALSE(xrpl::isXRP(result->takerPays.get<xrpl::Issue>().account));
}

TEST(BookOffersSpec, mpt_issuance_id_parses_as_mpt_issue)
{
    auto const mpt = std::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId);
    auto const result = parseBookOffers(request(mpt, mpt));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;

    ASSERT_TRUE(result->takerGets.holds<xrpl::MPTIssue>());
    ASSERT_TRUE(result->takerPays.holds<xrpl::MPTIssue>());

    xrpl::MPTID expected{};
    ASSERT_TRUE(expected.parseHex(kMptId));
    EXPECT_EQ(result->takerGets.get<xrpl::MPTIssue>().getMptID(), expected);
    EXPECT_EQ(result->takerPays.get<xrpl::MPTIssue>().getMptID(), expected);
}

// --- the taker objects ------------------------------------------------------

TEST(BookOffersSpec, missing_taker_names_the_field)
{
    expectError(
        std::format(R"JSON({{"taker_gets": {}}})JSON", kXrp),
        rpc::XrpldError::RpcInvalidParams,
        "Missing field 'taker_pays'.");
    expectError(
        std::format(R"JSON({{"taker_pays": {}}})JSON", kXrp),
        rpc::XrpldError::RpcInvalidParams,
        "Missing field 'taker_gets'.");
}

TEST(BookOffersSpec, taker_pays_is_checked_before_taker_gets)
{
    expectError(R"JSON({})JSON", rpc::XrpldError::RpcInvalidParams, "Missing field 'taker_pays'.");
    expectError(
        request("1", "1"),
        rpc::XrpldError::RpcInvalidParams,
        "Invalid field 'taker_pays', not object.");
}

TEST(BookOffersSpec, non_object_taker_is_not_object)
{
    for (auto const* taker : {"1", R"("XRP")", "[]", "true"})
    {
        expectTakerError(
            taker,
            rpc::XrpldError::RpcInvalidParams,
            rpc::XrpldError::RpcInvalidParams,
            "Invalid field '{}', not object.");
    }
}

TEST(BookOffersSpec, null_taker_is_missing_its_currency)
{
    expectTakerError(
        "null",
        rpc::XrpldError::RpcInvalidParams,
        rpc::XrpldError::RpcInvalidParams,
        "Missing field '{}.currency'.");
}

TEST(BookOffersSpec, neither_currency_nor_mpt_issuance_id_fails)
{
    expectTakerError(
        "{}",
        rpc::XrpldError::RpcInvalidParams,
        rpc::XrpldError::RpcInvalidParams,
        "Missing field '{}.currency'.");
}

TEST(BookOffersSpec, mpt_issuance_id_with_currency_or_issuer_fails)
{
    expectTakerError(
        std::format(R"JSON({{"currency": "USD", "mpt_issuance_id": "{}"}})JSON", kMptId),
        rpc::XrpldError::RpcInvalidParams,
        rpc::XrpldError::RpcInvalidParams,
        "Invalid field '{}'.");
    expectTakerError(
        std::format(R"JSON({{"mpt_issuance_id": "{}", "issuer": "{}"}})JSON", kMptId, kAcct1),
        rpc::XrpldError::RpcInvalidParams,
        rpc::XrpldError::RpcInvalidParams,
        "Invalid field '{}'.");
}

TEST(BookOffersSpec, non_string_currency_or_mpt_issuance_id_names_currency)
{
    expectTakerError(
        R"JSON({"currency": 123})JSON",
        rpc::XrpldError::RpcInvalidParams,
        rpc::XrpldError::RpcInvalidParams,
        "Invalid field '{}.currency', not string.");
    expectTakerError(
        R"JSON({"mpt_issuance_id": true})JSON",
        rpc::XrpldError::RpcInvalidParams,
        rpc::XrpldError::RpcInvalidParams,
        "Invalid field '{}.currency', not string.");
}

TEST(BookOffersSpec, malformed_mpt_issuance_id)
{
    expectTakerError(
        R"JSON({"mpt_issuance_id": "NOTAHEX"})JSON",
        rpc::XrpldError::RpcSrcCurMalformed,
        rpc::XrpldError::RpcDstAmtMalformed,
        "Invalid field '{}.mpt_issuance_id'");
}

// --- currency and issuer values --------------------------------------------

TEST(BookOffersSpec, bad_currency)
{
    expectTakerError(
        R"JSON({"currency": "TOOLONGCURRENCY"})JSON",
        rpc::XrpldError::RpcSrcCurMalformed,
        rpc::XrpldError::RpcDstAmtMalformed,
        "Invalid field '{}.currency', bad currency.");
}

TEST(BookOffersSpec, non_string_issuer_is_invalid_params)
{
    for (auto const* issuer : {"1", "null"})
    {
        expectTakerError(
            std::format(R"JSON({{"currency": "USD", "issuer": {}}})JSON", issuer),
            rpc::XrpldError::RpcInvalidParams,
            rpc::XrpldError::RpcInvalidParams,
            "Invalid field '{}.issuer', not string.");
    }
}

TEST(BookOffersSpec, bad_issuer)
{
    expectTakerError(
        R"JSON({"currency": "USD", "issuer": "notanaccount"})JSON",
        rpc::XrpldError::RpcSrcIsrMalformed,
        rpc::XrpldError::RpcDstIsrMalformed,
        "Invalid field '{}.issuer', bad issuer.");
    expectTakerError(
        std::format(R"JSON({{"currency": "USD", "issuer": "{}"}})JSON", kAccountOne),
        rpc::XrpldError::RpcSrcIsrMalformed,
        rpc::XrpldError::RpcDstIsrMalformed,
        "Invalid field '{}.issuer', bad issuer account one.");
}

TEST(BookOffersSpec, xrp_with_an_issuer_is_unneeded)
{
    expectTakerError(
        std::format(R"JSON({{"currency": "XRP", "issuer": "{}"}})JSON", kAcct1),
        rpc::XrpldError::RpcSrcIsrMalformed,
        rpc::XrpldError::RpcDstIsrMalformed,
        "Unneeded field '{}.issuer' for XRP currency specification.");
}

TEST(BookOffersSpec, issued_currency_without_a_non_xrp_issuer)
{
    for (auto const& taker : {
             std::string{R"JSON({"currency": "USD"})JSON"},
             std::format(R"JSON({{"currency": "USD", "issuer": "{}"}})JSON", kAccountZero),
         })
    {
        expectTakerError(
            taker,
            rpc::XrpldError::RpcSrcIsrMalformed,
            rpc::XrpldError::RpcDstIsrMalformed,
            "Invalid field '{}.issuer', expected non-XRP issuer.");
    }
}

// --- taker ------------------------------------------------------------------

TEST(BookOffersSpec, taker)
{
    auto const result =
        parseBookOffers(request(kXrp, kXrp, std::format(R"JSON(, "taker": "{}")JSON", kAcct1)));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_TRUE(result->taker.has_value());

    for (auto const* taker : {"1", "null"})
    {
        expectError(
            request(kXrp, kXrp, std::format(R"JSON(, "taker": {})JSON", taker)),
            rpc::XrpldError::RpcInvalidParams,
            "Invalid field 'taker', not string.");
    }
    for (auto const* taker : {"notanaccount", kPublicKeyHex})
    {
        expectError(
            request(kXrp, kXrp, std::format(R"JSON(, "taker": "{}")JSON", taker)),
            rpc::XrpldError::RpcInvalidParams,
            "Invalid field 'taker'.");
    }
}

// --- domain -----------------------------------------------------------------

TEST(BookOffersSpec, bad_domain_is_domain_malformed)
{
    for (auto const* domain : {"1", "null", R"("NOTHEX")"})
    {
        expectError(
            request(kXrp, kXrp, std::format(R"JSON(, "domain": {})JSON", domain)),
            rpc::XrpldError::RpcDomainMalformed,
            "Unable to parse domain.");
    }
}

// --- limit ------------------------------------------------------------------

TEST(BookOffersSpec, limit)
{
    for (auto const& [limit, expected] : {
             std::pair{"", handlers::book_offers::kLimitDefault},
             std::pair{R"(, "limit": null)", handlers::book_offers::kLimitDefault},
             std::pair{R"(, "limit": 1)", 1u},
             std::pair{R"(, "limit": 1000)", 1000u},
         })
    {
        auto const result = parseBookOffers(request(kXrp, kXrp, limit));
        ASSERT_TRUE(result.has_value()) << limit << " msg: " << result.error().message;
        EXPECT_EQ(result->limit, expected) << limit;
    }

    for (auto const* limit : {R"("1")", "-1", "2.5", "true"})
    {
        expectError(
            request(kXrp, kXrp, std::format(R"JSON(, "limit": {})JSON", limit)),
            rpc::XrpldError::RpcInvalidParams,
            "Invalid field 'limit', not unsigned integer.");
    }
    expectError(
        request(kXrp, kXrp, R"JSON(, "limit": 0)JSON"),
        rpc::XrpldError::RpcInvalidParams,
        "Invalid field 'limit'.");
}
