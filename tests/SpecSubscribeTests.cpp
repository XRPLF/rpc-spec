/** @file
 *  GTest coverage for the `subscribe` and `unsubscribe` typed specs.
 *  Compiled under RPCSPEC_IS_RIPPLED (the default for rpcspec_tests).
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

constexpr char const* kACCT1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr char const* kACCT2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

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

// ---------------------------------------------------------------------------
// 1. accounts array — two valid accounts (streams included to satisfy validator)
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, AccountsTwoElements)
{
    auto const r = parseSub(R"JSON({
        "streams": ["ledger"],
        "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"]
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->accounts.has_value());
    EXPECT_EQ(r->accounts->size(), 2u);

    auto const expected0 = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expected0.has_value());
    EXPECT_EQ((*r->accounts)[0], *expected0);

    auto const expected1 = rpc::spec::detail::accountFromStringStrict(kACCT2);
    ASSERT_TRUE(expected1.has_value());
    EXPECT_EQ((*r->accounts)[1], *expected1);
}

// ---------------------------------------------------------------------------
// 2. accounts_proposed array — two valid accounts
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, AccountsProposedTwoElements)
{
    auto const r = parseSub(R"JSON({
        "streams": ["ledger"],
        "accounts_proposed": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"]
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->accountsProposed.has_value());
    EXPECT_EQ(r->accountsProposed->size(), 2u);

    auto const expected0 = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expected0.has_value());
    EXPECT_EQ((*r->accountsProposed)[0], *expected0);
}

// ---------------------------------------------------------------------------
// 3. streams: ["ledger", "validations", "book_changes"] → 3 enum values
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, StreamsThreeCommon)
{
    auto const r = parseSub(R"JSON({"streams": ["ledger", "validations", "book_changes"]})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->streams.has_value());
    ASSERT_EQ(r->streams->size(), 3u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*r->streams)[0], ST::Ledger);
    EXPECT_EQ((*r->streams)[1], ST::Validations);
    EXPECT_EQ((*r->streams)[2], ST::BookChanges);
}

// ---------------------------------------------------------------------------
// 4. streams: deprecated alias rt_transactions → TransactionsProposed
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, StreamsDeprecatedRtTransactionsAlias)
{
    auto const r = parseSub(R"JSON({"streams": ["rt_transactions"]})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->streams.has_value());
    ASSERT_EQ(r->streams->size(), 1u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*r->streams)[0], ST::TransactionsProposed);
}

// ---------------------------------------------------------------------------
// 5. streams: ["server"] → StreamType::Server (rippled build)
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, StreamsServerAcceptedInRippledBuild)
{
    auto const r = parseSub(R"JSON({"streams": ["server"]})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->streams.has_value());
    ASSERT_EQ(r->streams->size(), 1u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*r->streams)[0], ST::Server);
}

// ---------------------------------------------------------------------------
// 6. streams: ["consensus"] → StreamType::Consensus (rippled build)
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, StreamsConsensusAcceptedInRippledBuild)
{
    auto const r = parseSub(R"JSON({"streams": ["consensus"]})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->streams.has_value());
    ASSERT_EQ(r->streams->size(), 1u);

    using ST = handlers::subscribe::StreamType;
    EXPECT_EQ((*r->streams)[0], ST::Consensus);
}

// ---------------------------------------------------------------------------
// 7. streams: unknown value → parse fails
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, StreamsBogusValueFails)
{
    auto const r = parseSub(R"JSON({"streams": ["bogus"]})JSON");
    EXPECT_FALSE(r.has_value());
}

// ---------------------------------------------------------------------------
// 8. streams: not an array → parse fails
// ---------------------------------------------------------------------------
TEST(SubscribeSpec, StreamsNotArrayFails)
{
    auto const r = parseSub(R"JSON({"streams": "ledger"})JSON");
    EXPECT_FALSE(r.has_value());
}

// ---------------------------------------------------------------------------
// 9. Dump test (subscribe): field keys + stream values appear in schema dump
// ---------------------------------------------------------------------------
TEST(SubscribeDump, FieldKeysAndStreamValuesPresent)
{
    std::ostringstream oss;
    rpc::spec::SpecDumpWriter w{oss};
    handlers::subscribe::kInputSpec.dump(w);
    auto const s = oss.str();

    static constexpr auto npos = std::string::npos;

    // Top-level field keys
    EXPECT_NE(s.find("accounts"), npos) << "missing: accounts";
    EXPECT_NE(s.find("streams"), npos) << "missing: streams";
    EXPECT_NE(s.find("accounts_proposed"), npos) << "missing: accounts_proposed";
    EXPECT_NE(s.find("books"), npos) << "missing: books";

    // Stream names exposed by StreamsValidator::describeParams
    EXPECT_NE(s.find("ledger"), npos) << "missing: ledger";
    EXPECT_NE(s.find("transactions_proposed"), npos) << "missing: transactions_proposed";
    EXPECT_NE(s.find("validations"), npos) << "missing: validations";
    EXPECT_NE(s.find("book_changes"), npos) << "missing: book_changes";
    EXPECT_NE(s.find("manifests"), npos) << "missing: manifests";

    // oneOf/alsoAllowed grouping key
    EXPECT_NE(s.find("oneOf"), npos) << "missing: oneOf";
}

// ---------------------------------------------------------------------------
// 10. Unsubscribe: accounts two elements (streams included to satisfy validator)
// ---------------------------------------------------------------------------
TEST(UnsubscribeSpec, AccountsTwoElements)
{
    auto const r = parseUnsub(R"JSON({
        "streams": ["ledger"],
        "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"]
    })JSON");
    ASSERT_TRUE(r.has_value()) << "error: " << r.error().error << " msg: " << r.error().message;
    ASSERT_TRUE(r->accounts.has_value());
    EXPECT_EQ(r->accounts->size(), 2u);

    auto const expected0 = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expected0.has_value());
    EXPECT_EQ((*r->accounts)[0], *expected0);
}

// ---------------------------------------------------------------------------
// 11. Unsubscribe: streams enum mapping
// ---------------------------------------------------------------------------
TEST(UnsubscribeSpec, StreamsEnumMapping)
{
    auto const r = parseUnsub(R"JSON({"streams": ["ledger", "transactions", "manifests"]})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->streams.has_value());
    ASSERT_EQ(r->streams->size(), 3u);

    using ST = handlers::unsubscribe::StreamType;
    EXPECT_EQ((*r->streams)[0], ST::Ledger);
    EXPECT_EQ((*r->streams)[1], ST::Transactions);
    EXPECT_EQ((*r->streams)[2], ST::Manifests);
}

// ---------------------------------------------------------------------------
// 12. Dump test (unsubscribe): stream values appear in schema dump
// ---------------------------------------------------------------------------
TEST(UnsubscribeDump, StreamValuesPresent)
{
    std::ostringstream oss;
    rpc::spec::SpecDumpWriter w{oss};
    handlers::unsubscribe::kInputSpec.dump(w);
    auto const s = oss.str();

    static constexpr auto npos = std::string::npos;

    EXPECT_NE(s.find("streams"), npos) << "missing: streams";
    EXPECT_NE(s.find("ledger"), npos) << "missing: ledger";
    EXPECT_NE(s.find("transactions_proposed"), npos) << "missing: transactions_proposed";
    EXPECT_NE(s.find("validations"), npos) << "missing: validations";
    EXPECT_NE(s.find("book_changes"), npos) << "missing: book_changes";
    EXPECT_NE(s.find("oneOf"), npos) << "missing: oneOf";
}
