/**
 * @file
 *  GTest coverage for the `subscribe` and `unsubscribe` typed specs.
 *  Compiled under RPCSPEC_IS_XRPLD (the default for rpcspec_tests).
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/subscribe/Spec.hpp>
#include <rpcspec/handlers/subscribe/Types.hpp>
#include <rpcspec/handlers/unsubscribe/Spec.hpp>
#include <rpcspec/handlers/unsubscribe/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <sstream>
#include <string>

using namespace rpc::spec;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

auto
parseSub(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::subscribe::kInputSpec.parse(value);
}

auto
parseUnsub(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::unsubscribe::kInputSpec.parse(value);
}

}  // namespace

TEST(SubscribeSpec, AccountsTwoElements)
{
    auto const result = parseSub(R"JSON({
        "streams": ["ledger"],
        "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"]
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->accounts.has_value());
    EXPECT_EQ(result->accounts->size(), 2u);

    auto const expected0 = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expected0.has_value());
    EXPECT_EQ((*result->accounts)[0], *expected0);

    auto const expected1 = rpc::spec::detail::accountFromStringStrict(kAcct2);
    ASSERT_TRUE(expected1.has_value());
    EXPECT_EQ((*result->accounts)[1], *expected1);
}

TEST(SubscribeSpec, AccountsProposedTwoElements)
{
    auto const result = parseSub(R"JSON({
        "streams": ["ledger"],
        "accounts_proposed": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"]
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->accountsProposed.has_value());
    EXPECT_EQ(result->accountsProposed->size(), 2u);

    auto const expected0 = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expected0.has_value());
    EXPECT_EQ((*result->accountsProposed)[0], *expected0);
}

TEST(SubscribeSpec, StreamsThreeCommon)
{
    auto const result =
        parseSub(R"JSON({"streams": ["ledger", "validations", "book_changes"]})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->streams.has_value());
    ASSERT_EQ(result->streams->size(), 3u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*result->streams)[0], ST::Ledger);
    EXPECT_EQ((*result->streams)[1], ST::Validations);
    EXPECT_EQ((*result->streams)[2], ST::BookChanges);
}

TEST(SubscribeSpec, StreamsDeprecatedRtTransactionsAlias)
{
    auto const result = parseSub(R"JSON({"streams": ["rt_transactions"]})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->streams.has_value());
    ASSERT_EQ(result->streams->size(), 1u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*result->streams)[0], ST::TransactionsProposed);
}

TEST(SubscribeSpec, StreamsServerAcceptedInRippledBuild)
{
    auto const result = parseSub(R"JSON({"streams": ["server"]})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->streams.has_value());
    ASSERT_EQ(result->streams->size(), 1u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*result->streams)[0], ST::Server);
}

TEST(SubscribeSpec, StreamsConsensusAcceptedInRippledBuild)
{
    auto const result = parseSub(R"JSON({"streams": ["consensus"]})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->streams.has_value());
    ASSERT_EQ(result->streams->size(), 1u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*result->streams)[0], ST::Consensus);
}

TEST(SubscribeSpec, StreamsBogusValueFails)
{
    auto const result = parseSub(R"JSON({"streams": ["bogus"]})JSON");
    EXPECT_FALSE(result.has_value());
}

TEST(SubscribeSpec, StreamsNotArrayFails)
{
    auto const result = parseSub(R"JSON({"streams": "ledger"})JSON");
    EXPECT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// `books` / kBooksValidator
//
// The validator is shared verbatim by subscribe and unsubscribe. Note these
// assert the *Status* the spec produces, not the rendered wire message: a bare
// `Status{code}` carries an empty `message`, and the consumer (Clio/xrpld)
// renders the canonical text from the error table at response time. Asserting
// the emptiness is deliberate — it is what stops an explicit override being
// reintroduced.
// ---------------------------------------------------------------------------

TEST(SubscribeSpec, BooksValidPairParses)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "XRP"},
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                }
            }
        ]
    })JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->books.has_value());
    EXPECT_EQ(result->books->size(), 1u);
}

TEST(SubscribeSpec, BooksIdenticalAssetsIsBadMarketWithNoMessageOverride)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "XRP"},
                "taker_gets": {"currency": "XRP"}
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcBadMarket);
    // No override: the consumer renders "No such market." from the error table.
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}

TEST(SubscribeSpec, BooksIdenticalIouAndIssuerIsBadMarket)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {
                    "currency": "USD",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                },
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                }
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcBadMarket);
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}

TEST(SubscribeSpec, BooksMissingTakerPays)
{
    auto const result = parseSub(R"JSON({"books": [{"taker_gets": {"currency": "XRP"}}]})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'taker_pays'");
}

TEST(SubscribeSpec, BooksTakerPaysNotObject)
{
    auto const result = parseSub(R"JSON({
        "books": [{"taker_pays": "XRP", "taker_gets": {"currency": "XRP"}}]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Field 'taker_pays' is not an object");
}

TEST(SubscribeSpec, BooksMissingTakerGets)
{
    auto const result = parseSub(R"JSON({"books": [{"taker_pays": {"currency": "XRP"}}]})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'taker_gets'");
}

TEST(SubscribeSpec, BooksPaysCurrencyMissingIsSrcCurMalformed)
{
    auto const result = parseSub(R"JSON({
        "books": [{"taker_pays": {}, "taker_gets": {"currency": "XRP"}}]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcSrcCurMalformed);
}

TEST(SubscribeSpec, BooksGetsCurrencyMissingIsDstAmtMalformed)
{
    auto const result = parseSub(R"JSON({
        "books": [{"taker_pays": {"currency": "XRP"}, "taker_gets": {}}]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcDstAmtMalformed);
}

TEST(SubscribeSpec, BooksUnneededPaysIssuerForXrp)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {
                    "currency": "XRP",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                },
                "taker_gets": {"currency": "XRP"}
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcSrcIsrMalformed);
    EXPECT_EQ(
        result.error().message,
        "Unneeded field 'taker_pays.issuer' for XRP currency specification.");
}

TEST(SubscribeSpec, BooksNonXrpPaysWithoutIssuerIsSrcIsrMalformed)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "USD"},
                "taker_gets": {"currency": "XRP"}
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcSrcIsrMalformed);
    EXPECT_EQ(
        result.error().message, "Invalid field 'taker_pays.issuer', expected non-XRP issuer.");
}

TEST(SubscribeSpec, BooksGetsIssuerAccountOneIsRejected)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "XRP"},
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rrrrrrrrrrrrrrrrrrrrBZbvji"
                }
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcDstIsrMalformed);
    EXPECT_EQ(result.error().message, "Invalid field 'taker_gets.issuer', bad issuer account one.");
}

TEST(SubscribeSpec, BooksDomainNotStringIsDomainMalformed)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "XRP"},
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                },
                "domain": 123
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcDomainMalformed);
}

TEST(SubscribeSpec, BooksDomainNotHexIsDomainMalformed)
{
    auto const result = parseSub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "XRP"},
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                },
                "domain": "notavalidhex"
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcDomainMalformed);
}

TEST(SubscribeDump, FieldKeysAndStreamValuesPresent)
{
    std::ostringstream oss;
    rpc::spec::SpecDumpWriter writer{oss};
    handlers::subscribe::kInputSpec.dump(writer);
    auto const text = oss.str();

    static constexpr auto npos = std::string::npos;

    EXPECT_NE(text.find("accounts"), npos) << "missing: accounts";
    EXPECT_NE(text.find("streams"), npos) << "missing: streams";
    EXPECT_NE(text.find("accounts_proposed"), npos) << "missing: accounts_proposed";
    EXPECT_NE(text.find("books"), npos) << "missing: books";
    EXPECT_NE(text.find("ledger"), npos) << "missing: ledger";
    EXPECT_NE(text.find("transactions_proposed"), npos) << "missing: transactions_proposed";
    EXPECT_NE(text.find("validations"), npos) << "missing: validations";
    EXPECT_NE(text.find("book_changes"), npos) << "missing: book_changes";
    EXPECT_NE(text.find("manifests"), npos) << "missing: manifests";
    EXPECT_NE(text.find("oneOf"), npos) << "missing: oneOf";
}

TEST(UnsubscribeSpec, AccountsTwoElements)
{
    auto const result = parseUnsub(R"JSON({
        "streams": ["ledger"],
        "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"]
    })JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->accounts.has_value());
    EXPECT_EQ(result->accounts->size(), 2u);

    auto const expected0 = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expected0.has_value());
    EXPECT_EQ((*result->accounts)[0], *expected0);
}

TEST(UnsubscribeSpec, StreamsEnumMapping)
{
    auto const result =
        parseUnsub(R"JSON({"streams": ["ledger", "transactions", "manifests"]})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->streams.has_value());
    ASSERT_EQ(result->streams->size(), 3u);

    using ST = handlers::unsubscribe::StreamType;
    EXPECT_EQ((*result->streams)[0], ST::Ledger);
    EXPECT_EQ((*result->streams)[1], ST::Transactions);
    EXPECT_EQ((*result->streams)[2], ST::Manifests);
}

TEST(UnsubscribeSpec, BooksValidPairParses)
{
    auto const result = parseUnsub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "XRP"},
                "taker_gets": {
                    "currency": "USD",
                    "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                }
            }
        ]
    })JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->books.has_value());
    EXPECT_EQ(result->books->size(), 1u);
}

TEST(UnsubscribeSpec, BooksIdenticalAssetsIsBadMarketWithNoMessageOverride)
{
    auto const result = parseUnsub(R"JSON({
        "books": [
            {
                "taker_pays": {"currency": "XRP"},
                "taker_gets": {"currency": "XRP"}
            }
        ]
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcBadMarket);
    // No override: the consumer renders "No such market." from the error table.
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}

TEST(UnsubscribeSpec, BooksMissingTakerGets)
{
    auto const result = parseUnsub(R"JSON({"books": [{"taker_pays": {"currency": "XRP"}}]})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'taker_gets'");
}

TEST(UnsubscribeDump, StreamValuesPresent)
{
    std::ostringstream oss;
    rpc::spec::SpecDumpWriter writer{oss};
    handlers::unsubscribe::kInputSpec.dump(writer);
    auto const text = oss.str();

    static constexpr auto npos = std::string::npos;

    EXPECT_NE(text.find("streams"), npos) << "missing: streams";
    EXPECT_NE(text.find("ledger"), npos) << "missing: ledger";
    EXPECT_NE(text.find("transactions_proposed"), npos) << "missing: transactions_proposed";
    EXPECT_NE(text.find("validations"), npos) << "missing: validations";
    EXPECT_NE(text.find("book_changes"), npos) << "missing: book_changes";
    EXPECT_NE(text.find("oneOf"), npos) << "missing: oneOf";
}
