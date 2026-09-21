#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/mpt_holders/Spec.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>

#include <cstddef>
#include <expected>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::mpt_holders;

namespace {

constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kMptId = "000004C463C52827307480341125DA0577DEFC38405B0E3E";
// `marker` is a hex-encoded AccountID (20 bytes), not base58.
constexpr auto kMarker = "0102030405060708090A0B0C0D0E0F1011121314";

/**
 * @brief Parses @p json through the mpt_holders spec.
 */
[[nodiscard]] std::expected<Input, rpc::Status>
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

/**
 * @brief A request naming only the required mpt_issuance_id, plus @p extra.
 */
[[nodiscard]] std::string
request(std::string const& extra)
{
    return R"({"mpt_issuance_id": ")" + std::string{kMptId} + R"(")" + extra + "}";
}

}  // namespace

TEST(MPTHoldersSpec, AccountsAbsentLeavesFilterUnset)
{
    auto const result = parse(request(""));
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->accounts.has_value());
}

TEST(MPTHoldersSpec, AccountsParsedIntoAccountIdVector)
{
    auto const result = parse(
        request(R"(, "accounts": [")" + std::string{kAccount} + R"(", ")" + kAccount2 + R"("])"));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->accounts.has_value());
    EXPECT_EQ(result->accounts->size(), 2u);
    EXPECT_NE(result->accounts->at(0), result->accounts->at(1));
}

TEST(MPTHoldersSpec, AccountsRejectsNonArray)
{
    auto const result = parse(request(R"(, "accounts": "notanarray")"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'accounts', not array.");
}

TEST(MPTHoldersSpec, AccountsRejectsEmptyArray)
{
    auto const result = parse(request(R"(, "accounts": [])"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().message, "Invalid field 'accounts', not an array of 1 to 100 account IDs.");
}

TEST(MPTHoldersSpec, AccountsRejectsMoreThanTheBound)
{
    std::string accounts;
    for (auto i = 0uz; i <= kMaxAccounts; ++i)
        accounts += (i == 0 ? "\"" : ", \"") + std::string{kAccount} + "\"";

    auto const result = parse(request(R"(, "accounts": [)" + accounts + "]"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().message, "Invalid field 'accounts', not an array of 1 to 100 account IDs.");
}

TEST(MPTHoldersSpec, AccountsAcceptsExactlyTheBound)
{
    std::string accounts;
    for (auto i = 0uz; i < kMaxAccounts; ++i)
        accounts += (i == 0 ? "\"" : ", \"") + std::string{kAccount} + "\"";

    auto const result = parse(request(R"(, "accounts": [)" + accounts + "]"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->accounts->size(), kMaxAccounts);
}

TEST(MPTHoldersSpec, AccountsRejectsNonStringElement)
{
    auto const result = parse(request(R"(, "accounts": [1])"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'accounts', not an array of account IDs.");
}

TEST(MPTHoldersSpec, AccountsRejectsMalformedElement)
{
    auto const result = parse(request(R"(, "accounts": ["notanaccount"])"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'accounts', not an array of account IDs.");
}

// `accounts` cannot be combined with paging, but that rule needs to see which keys the request
// actually named, so it lives in the handler - the spec only makes the distinction possible by
// leaving `limit` unset when absent.

TEST(MPTHoldersSpec, MarkerAndAccountsBothParseSoTheHandlerCanRejectThePair)
{
    auto const result = parse(request(
        R"(, "accounts": [")" + std::string{kAccount} + R"("], "marker": ")" + kMarker + R"(")"));
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->accounts.has_value());
    EXPECT_TRUE(result->marker.has_value());
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
    auto const result = parse(request(R"(, "limit": 99999)"));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, kLimitMax);
}
