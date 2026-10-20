/**
 * @file
 *  GTest coverage for the `account_currencies` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  xrpld reads `ident` when `account` is absent, and decodes the account only
 *  after it has resolved the ledger, so a malformed one reaches the handler as
 *  a `DeferredError`. The Clio arm is pinned by SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_currencies/Spec.hpp>
#include <rpcspec/handlers/account_currencies/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <initializer_list>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_currencies;

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

std::string
withMarker(std::string const& marker)
{
    return req(std::format(R"JSON(, "marker": "{}")JSON", marker));
}

}  // namespace

TEST(AccountCurrenciesSpec, account_parses)
{
    auto const result = parse(std::format(R"JSON({{"account": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpec, ident_is_read_when_account_is_absent)
{
    auto const result = parse(std::format(R"JSON({{"ident": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpec, ident_is_ignored_when_account_is_present)
{
    auto const result = parse(std::format(R"JSON({{"account": "{}", "ident": 5}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpec, neither_is_a_missing_account)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'account'.");
}

TEST(AccountCurrenciesSpec, non_string_is_reported_under_its_own_key)
{
    for (auto const* key : {"account", "ident"})
    {
        for (auto const* bad : {"1", "1.1", "true", "null", "{}", "[]"})
        {
            auto const result = parse(std::format(R"JSON({{"{}": {}}})JSON", key, bad));
            ASSERT_FALSE(result.has_value()) << key << "=" << bad;
            EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << key << "=" << bad;
            EXPECT_EQ(result.error().message, std::format("Invalid field '{}'.", key))
                << key << "=" << bad;
        }
    }
}

TEST(AccountCurrenciesSpec, malformed_account_is_left_to_the_handler)
{
    for (auto const* key : {"account", "ident"})
    {
        auto const result = parse(std::format(R"JSON({{"{}": "llIIOO"}})JSON", key));
        ASSERT_TRUE(result.has_value())
            << "error: " << result.error().error << " msg: " << result.error().message;
        ASSERT_FALSE(result->account.has_value()) << key;
        EXPECT_EQ(result->account.error(), DeferredError::Malformed) << key;
    }
}

TEST(AccountCurrenciesSpec, absent_limit_is_the_default)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
    EXPECT_FALSE(result->marker.has_value());
}

TEST(AccountCurrenciesSpec, limit_is_not_clamped_by_the_spec)
{
    auto const low = parse(req(R"JSON(, "limit": 1)JSON"));
    ASSERT_TRUE(low.has_value()) << "error: " << low.error().error
                                 << " msg: " << low.error().message;
    EXPECT_EQ(low->limit, 1u);

    auto const high = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(high.has_value());
    EXPECT_EQ(high->limit, 100000u);
}

TEST(AccountCurrenciesSpec, null_limit_is_the_default)
{
    auto const result = parse(req(R"JSON(, "limit": null)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
}

TEST(AccountCurrenciesSpec, limit_zero_is_rejected)
{
    auto const result = parse(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'limit'.");
}

TEST(AccountCurrenciesSpec, non_integer_limit_is_rejected)
{
    for (auto const* bad : {R"JSON("0")JSON", "-1", "1.5", "true"})
    {
        auto const result = parse(req(std::format(R"JSON(, "limit": {})JSON", bad)));
        ASSERT_FALSE(result.has_value()) << "limit=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "limit=" << bad;
        EXPECT_EQ(result.error().message, "Invalid field 'limit', not unsigned integer.")
            << "limit=" << bad;
    }
}

TEST(AccountCurrenciesSpec, well_formed_marker_round_trips_as_string)
{
    auto const marker = std::format("{},7", kHex1);
    auto const result = parse(withMarker(marker));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(*result->marker, marker);
}

TEST(AccountCurrenciesSpec, non_string_marker_names_the_field)
{
    auto const result = parse(req(R"JSON(, "marker": 5)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker', not string.");
}

TEST(AccountCurrenciesSpec, malformed_marker_is_invalid_params)
{
    for (auto const& bad : {
             std::string{kHex1},
             std::string{"NOTHEX,7"},
             std::format("{},", kHex1),
             std::format("{},abc", kHex1),
             std::format("{},7x", kHex1),
             std::format("{},-1", kHex1),
         })
    {
        auto const result = parse(withMarker(bad));
        ASSERT_FALSE(result.has_value()) << "marker=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_TRUE(result.error().message.empty()) << "marker=" << bad;
    }
}

TEST(AccountCurrenciesSpec, marker_hint_may_have_a_leading_plus)
{
    auto const result = parse(withMarker(std::format("{},+7", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AccountCurrenciesSpec, marker_text_after_a_second_comma_is_ignored)
{
    auto const result = parse(withMarker(std::format("{},7,junk", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AccountCurrenciesSpec, deprecated_fields_do_not_warn)
{
    auto const request = boost::json::parse(
        std::format(R"JSON({{"account": "{}", "account_index": 1, "strict": true}})JSON", kAcct1));
    EXPECT_TRUE(kInputSpec.check(request).empty());
}
