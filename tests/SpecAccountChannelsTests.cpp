/**
 * @file
 *  GTest coverage for the `account_channels` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  An empty `destination_account` is no filter at all on xrpld. The Clio arm is
 *  pinned by SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_channels/Spec.hpp>
#include <rpcspec/handlers/account_channels/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <initializer_list>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_channels;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
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

TEST(AccountChannelsSpec, account_required)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'account'.");
}

TEST(AccountChannelsSpec, minimal_request_parses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->destinationAccount.has_value());
    EXPECT_EQ(result->limit, kLimitDefault);
    EXPECT_FALSE(result->marker.has_value());
}

// --- account / destination_account ------------------------------------------

TEST(AccountChannelsSpec, non_string_account_is_invalid_params)
{
    auto const result = parse(R"JSON({"account": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'account'.");
}

TEST(AccountChannelsSpec, malformed_account_is_act_malformed)
{
    auto const result = parse(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(AccountChannelsSpec, destination_account_parses)
{
    auto const result = parse(req(std::format(R"JSON(, "destination_account": "{}")JSON", kAcct2)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->destinationAccount.has_value());
}

TEST(AccountChannelsSpec, empty_destination_account_is_no_destination)
{
    auto const result = parse(req(R"JSON(, "destination_account": "")JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->destinationAccount.has_value());
}

TEST(AccountChannelsSpec, non_string_destination_account_names_the_field)
{
    for (auto const* bad : {"1", "true", "null", "{}", "[]"})
    {
        auto const result = parse(req(std::format(R"JSON(, "destination_account": {})JSON", bad)));
        ASSERT_FALSE(result.has_value()) << "destination_account=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams)
            << "destination_account=" << bad;
        EXPECT_EQ(result.error().message, "Invalid field 'destination_account'.")
            << "destination_account=" << bad;
    }
}

TEST(AccountChannelsSpec, malformed_destination_account_is_act_malformed)
{
    auto const result = parse(req(R"JSON(, "destination_account": "notanaccount")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_TRUE(result.error().message.empty());
}

// --- limit ------------------------------------------------------------------

TEST(AccountChannelsSpec, limit_is_not_clamped_by_the_spec)
{
    auto const low = parse(req(R"JSON(, "limit": 1)JSON"));
    ASSERT_TRUE(low.has_value()) << "error: " << low.error().error
                                 << " msg: " << low.error().message;
    EXPECT_EQ(low->limit, 1u);

    auto const high = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(high.has_value());
    EXPECT_EQ(high->limit, 100000u);
}

TEST(AccountChannelsSpec, null_limit_is_the_default)
{
    auto const result = parse(req(R"JSON(, "limit": null)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
}

TEST(AccountChannelsSpec, limit_zero_is_rejected)
{
    auto const result = parse(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'limit'.");
}

TEST(AccountChannelsSpec, non_integer_limit_is_rejected)
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

// --- marker -----------------------------------------------------------------

TEST(AccountChannelsSpec, well_formed_marker_round_trips_as_string)
{
    auto const marker = std::format("{},7", kHex1);
    auto const result = parse(withMarker(marker));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(*result->marker, marker);
}

TEST(AccountChannelsSpec, non_string_marker_names_the_field)
{
    for (auto const* bad : {"5", "null"})
    {
        auto const result = parse(req(std::format(R"JSON(, "marker": {})JSON", bad)));
        ASSERT_FALSE(result.has_value()) << "marker=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_EQ(result.error().message, "Invalid field 'marker', not string.")
            << "marker=" << bad;
    }
}

TEST(AccountChannelsSpec, malformed_marker_is_invalid_params)
{
    for (auto const& bad : {
             std::string{kHex1},
             std::string{"NOTHEX,7"},
             std::format("{},", kHex1),
             std::format("{},abc", kHex1),
             std::format("{},7x", kHex1),
         })
    {
        auto const result = parse(withMarker(bad));
        ASSERT_FALSE(result.has_value()) << "marker=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_TRUE(result.error().message.empty()) << "marker=" << bad;
    }
}

TEST(AccountChannelsSpec, marker_hint_may_have_a_leading_plus)
{
    auto const result = parse(withMarker(std::format("{},+7", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AccountChannelsSpec, marker_text_after_a_second_comma_is_ignored)
{
    auto const result = parse(withMarker(std::format("{},7,junk", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}
