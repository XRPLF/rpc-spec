#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/mpt_holders/Spec.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>

#include <xrpl_mock.hpp>

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::mpt_holders;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kMPT_ID = "000004C463C52827307480341125DA0577DEFC38405B0E3E";
// `marker` is a hex-encoded AccountID (20 bytes), not base58.
constexpr auto kMARKER = "0102030405060708090A0B0C0D0E0F1011121314";

/** @brief Parses @p json through the mpt_holders spec. */
[[nodiscard]] std::expected<Input, rpc::Status>
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

/** @brief A request naming only the required mpt_issuance_id, plus @p extra. */
[[nodiscard]] std::string
request(std::string const& extra)
{
    return R"({"mpt_issuance_id": ")" + std::string{kMPT_ID} + R"(")" + extra + "}";
}

}  // namespace

TEST(MPTHoldersSpec, AccountsAbsentLeavesFilterUnset)
{
    auto const r = parse(request(""));
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->accounts.has_value());
}

TEST(MPTHoldersSpec, AccountsParsedIntoAccountIdVector)
{
    auto const r = parse(
        request(R"(, "accounts": [")" + std::string{kACCOUNT} + R"(", ")" + kACCOUNT2 + R"("])"));
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->accounts.has_value());
    EXPECT_EQ(r->accounts->size(), 2u);
    EXPECT_NE(r->accounts->at(0), r->accounts->at(1));
}

TEST(MPTHoldersSpec, AccountsRejectsNonArray)
{
    auto const r = parse(request(R"(, "accounts": "notanarray")"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Invalid field 'accounts', not array.");
}

TEST(MPTHoldersSpec, AccountsRejectsEmptyArray)
{
    auto const r = parse(request(R"(, "accounts": [])"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().message, "Invalid field 'accounts', not an array of 1 to 100 account IDs.");
}

TEST(MPTHoldersSpec, AccountsRejectsMoreThanTheBound)
{
    std::string accounts;
    for (std::size_t i = 0; i <= kMaxAccounts; ++i)
        accounts += (i == 0 ? "\"" : ", \"") + std::string{kACCOUNT} + "\"";

    auto const r = parse(request(R"(, "accounts": [)" + accounts + "]"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().message, "Invalid field 'accounts', not an array of 1 to 100 account IDs.");
}

TEST(MPTHoldersSpec, AccountsAcceptsExactlyTheBound)
{
    std::string accounts;
    for (std::size_t i = 0; i < kMaxAccounts; ++i)
        accounts += (i == 0 ? "\"" : ", \"") + std::string{kACCOUNT} + "\"";

    auto const r = parse(request(R"(, "accounts": [)" + accounts + "]"));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->accounts->size(), kMaxAccounts);
}

TEST(MPTHoldersSpec, AccountsRejectsNonStringElement)
{
    auto const r = parse(request(R"(, "accounts": [1])"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().message, "Invalid field 'accounts', not an array of account IDs.");
}

TEST(MPTHoldersSpec, AccountsRejectsMalformedElement)
{
    auto const r = parse(request(R"(, "accounts": ["notanaccount"])"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().message, "Invalid field 'accounts', not an array of account IDs.");
}

// `accounts` cannot be combined with paging, but that rule needs to see which keys the request
// actually named, so it lives in the handler - the spec only makes the distinction possible by
// leaving `limit` unset when absent.

TEST(MPTHoldersSpec, MarkerAndAccountsBothParseSoTheHandlerCanRejectThePair)
{
    auto const r = parse(request(
        R"(, "accounts": [")" + std::string{kACCOUNT} + R"("], "marker": ")" + kMARKER + R"(")"));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->accounts.has_value());
    EXPECT_TRUE(r->marker.has_value());
}

TEST(MPTHoldersSpec, LimitIsUnsetWhenAbsentAndSetWhenGiven)
{
    auto const absent = parse(request(""));
    ASSERT_TRUE(absent.has_value());
    EXPECT_FALSE(absent->limit.has_value());

    auto const given = parse(request(R"(, "limit": 10)"));
    ASSERT_TRUE(given.has_value());
    ASSERT_TRUE(given->limit.has_value());
    EXPECT_EQ(*given->limit, 10u);
}

TEST(MPTHoldersSpec, LimitStillClampedToTheMaximum)
{
    auto const r = parse(request(R"(, "limit": 99999)"));
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->limit.has_value());
    EXPECT_EQ(*r->limit, kLimitMax);
}
