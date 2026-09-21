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

#include <sstream>
#include <string>

using namespace rpc::spec;

namespace {

constexpr char const* kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr char const* kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

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
