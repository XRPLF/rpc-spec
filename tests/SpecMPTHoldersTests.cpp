#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/mpt_holders/Spec.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <expected>
#include <format>
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
    return std::format(R"({{"mpt_issuance_id": "{}"{}}})", kMptId, extra);
}

}  // namespace

TEST(MPTHoldersSpec, accounts_absent_leaves_filter_unset)
{
    auto const result = parse(request(""));
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->accounts.has_value());
}

TEST(MPTHoldersSpec, accounts_parsed_into_account_id_vector)
{
    auto const result =
        parse(request(std::format(R"(, "accounts": ["{}", "{}"])", kAccount, kAccount2)));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->accounts.has_value());
    EXPECT_EQ(result->accounts->size(), 2u);
    EXPECT_NE(result->accounts->at(0), result->accounts->at(1));
}

TEST(MPTHoldersSpec, accounts_rejects_non_array)
{
    auto const result = parse(request(R"(, "accounts": "notanarray")"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'accounts', not array.");
}

TEST(MPTHoldersSpec, accounts_rejects_empty_array)
{
    auto const result = parse(request(R"(, "accounts": [])"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().message, "Invalid field 'accounts', not an array of 1 to 100 account IDs.");
}

TEST(MPTHoldersSpec, accounts_rejects_more_than_the_bound)
{
    std::string accounts;
    for (auto i = 0uz; i <= kMaxAccounts; ++i)
        accounts += std::format("{}\"{}\"", i == 0 ? "" : ", ", kAccount);

    auto const result = parse(request(std::format(R"(, "accounts": [{}])", accounts)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().message, "Invalid field 'accounts', not an array of 1 to 100 account IDs.");
}

TEST(MPTHoldersSpec, accounts_accepts_exactly_the_bound)
{
    std::string accounts;
    for (auto i = 0uz; i < kMaxAccounts; ++i)
        accounts += std::format("{}\"{}\"", i == 0 ? "" : ", ", kAccount);

    auto const result = parse(request(std::format(R"(, "accounts": [{}])", accounts)));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->accounts->size(), kMaxAccounts);
}

TEST(MPTHoldersSpec, accounts_rejects_non_string_element)
{
    auto const result = parse(request(R"(, "accounts": [1])"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'accounts', not an array of account IDs.");
}

TEST(MPTHoldersSpec, accounts_rejects_malformed_element)
{
    auto const result = parse(request(R"(, "accounts": ["notanaccount"])"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'accounts', not an array of account IDs.");
}

// `accounts` cannot be combined with paging, but that rule needs to see which keys the request
// actually named, so it lives in the handler - the spec only makes the distinction possible by
// leaving `limit` unset when absent.

TEST(MPTHoldersSpec, marker_and_accounts_both_parse_so_the_handler_can_reject_the_pair)
{
    auto const result =
        parse(request(std::format(R"(, "accounts": ["{}"], "marker": "{}")", kAccount, kMarker)));
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->accounts.has_value());
    EXPECT_TRUE(result->marker.has_value());
}

TEST(MPTHoldersSpec, limit_is_unset_when_absent_and_set_when_given)
{
    auto const absent = parse(request(""));
    ASSERT_TRUE(absent.has_value());
    EXPECT_FALSE(absent->limit.has_value());

    auto const given = parse(request(R"(, "limit": 10)"));
    ASSERT_TRUE(given.has_value());
    ASSERT_TRUE(given->limit.has_value());
    EXPECT_EQ(*given->limit, 10u);
}

TEST(MPTHoldersSpec, limit_still_clamped_to_the_maximum)
{
    auto const result = parse(request(R"(, "limit": 99999)"));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, kLimitMax);
}
