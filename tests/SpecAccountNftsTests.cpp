/**
 * @file
 *  GTest coverage for the `account_nfts` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  xrpld clamps `limit` in the handler, and only for roles that are not
 *  unlimited, so the spec leaves it unclamped. The Clio arm is pinned by
 *  SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_nfts/Spec.hpp>
#include <rpcspec/handlers/account_nfts/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <format>
#include <initializer_list>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_nfts;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

std::string
req(std::string const& extra = {})
{
    return std::format(R"JSON({{"account": "{}"{}}})JSON", kAcct1, extra);
}

}  // namespace

TEST(AccountNftsSpec, minimal_request_parses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
    EXPECT_FALSE(result->marker.has_value());
}

TEST(AccountNftsSpec, missing_account)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'account'.");
}

TEST(AccountNftsSpec, non_string_account)
{
    for (auto const* bad : {"1", "1.1", "true", "null", "{}", "[]"})
    {
        auto const result = parse(std::format(R"JSON({{"account": {}}})JSON", bad));
        ASSERT_FALSE(result.has_value()) << "account=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "account=" << bad;
        EXPECT_EQ(result.error().message, "Invalid field 'account'.") << "account=" << bad;
    }
}

TEST(AccountNftsSpec, malformed_account_is_act_malformed)
{
    auto const result = parse(R"JSON({"account": "llIIOO"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(AccountNftsSpec, limit_is_not_clamped_by_the_spec)
{
    auto const result = parse(req(R"JSON(, "limit": 1)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, 1u);
}

TEST(AccountNftsSpec, null_limit_is_the_default)
{
    auto const result = parse(req(R"JSON(, "limit": null)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
}

TEST(AccountNftsSpec, limit_zero_is_rejected)
{
    auto const result = parse(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'limit'.");
}

TEST(AccountNftsSpec, non_integer_limit_is_rejected)
{
    auto const result = parse(req(R"JSON(, "limit": "10")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'limit', not unsigned integer.");
}

TEST(AccountNftsSpec, marker_parses)
{
    auto const result = parse(req(std::format(R"JSON(, "marker": "{}")JSON", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHex1));
    EXPECT_EQ(*result->marker, expected);
}

TEST(AccountNftsSpec, non_string_marker)
{
    auto const result = parse(req(R"JSON(, "marker": 5)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker', not string.");
}

TEST(AccountNftsSpec, non_hex_marker)
{
    auto const result = parse(req(R"JSON(, "marker": "DEADBEEF")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountNftsSpec, limit_is_checked_before_marker)
{
    auto const result = parse(req(R"JSON(, "limit": 0, "marker": 5)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'limit'.");
}
