/**
 * @file
 *  GTest coverage for the `account_lines` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  Covers the handler's two local converters (`AccountIdActMalformedConverter`,
 *  `AsBoolConverter`), the shared `accountMarker` cursor validator, and the
 *  min/clamp/default interaction on `limit`.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_lines/Spec.hpp>
#include <rpcspec/handlers/account_lines/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_lines;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV1.parse(value);
}

std::string
req(std::string const& extra = {})
{
    return std::format(R"JSON({{"account": "{}"{}}})JSON", kAcct1, extra);
}

}  // namespace

TEST(AccountLinesSpec, account_required)
{
    EXPECT_FALSE(parse(R"JSON({})JSON").has_value());
}

TEST(AccountLinesSpec, minimal_request_parses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->peer.has_value());
    EXPECT_FALSE(result->ignoreDefault);
    EXPECT_EQ(result->limit, kLimitDefault);
    EXPECT_FALSE(result->marker.has_value());
}

// --- AccountIdActMalformedConverter ----------------------------------------
// Note this handler's local converter reports RpcActMalformed for *both* a
// wrong JSON type and an unparseable string, unlike the shared accountId
// converter which distinguishes them.

TEST(AccountLinesSpec, malformed_account_is_act_malformed)
{
    auto const result = parse(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
}

TEST(AccountLinesSpec, non_string_account_is_also_act_malformed)
{
    auto const result = parse(R"JSON({"account": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
}

TEST(AccountLinesSpec, peer_parses)
{
    auto const result = parse(req(std::format(R"JSON(, "peer": "{}")JSON", kAcct2)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->peer.has_value());
}

TEST(AccountLinesSpec, malformed_peer_is_act_malformed)
{
    auto const result = parse(req(R"JSON(, "peer": "notanaccount")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
}

// --- AsBoolConverter --------------------------------------------------------

TEST(AccountLinesSpec, ignore_default_bool_parses)
{
    auto const result = parse(req(R"JSON(, "ignore_default": true)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->ignoreDefault);
}

TEST(AccountLinesSpec, ignore_default_non_bool_is_rejected)
{
    for (auto const* bad : {"1", R"("true")", "{}"})
    {
        auto const result = parse(req(std::format(R"JSON(, "ignore_default": {})JSON", bad)));
        ASSERT_FALSE(result.has_value()) << "ignore_default=" << bad << " unexpectedly accepted";
    }
}

// --- limit ------------------------------------------------------------------

TEST(AccountLinesSpec, limit_below_clamp_floor_is_raised)
{
    // min(1) admits it, then clamp(10, 400) raises it to the floor.
    auto const result = parse(req(R"JSON(, "limit": 5)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitMin);
}

TEST(AccountLinesSpec, limit_above_max_is_clamped)
{
    auto const result = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, kLimitMax);
}

TEST(AccountLinesSpec, limit_zero_is_rejected)
{
    auto const result = parse(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
}

// --- accountMarker ----------------------------------------------------------

TEST(AccountLinesSpec, well_formed_marker_parses)
{
    auto const result = parse(req(std::format(R"JSON(, "marker": "{},7")JSON", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
}

TEST(AccountLinesSpec, non_string_marker_names_the_field)
{
    auto const result = parse(req(R"JSON(, "marker": 5)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerNotString");
}

TEST(AccountLinesSpec, marker_without_comma_is_malformed_cursor)
{
    auto const result = parse(req(std::format(R"JSON(, "marker": "{}")JSON", kHex1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountLinesSpec, marker_with_bad_hex_is_malformed_cursor)
{
    auto const result = parse(req(R"JSON(, "marker": "NOTHEX,7")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountLinesSpec, marker_with_non_numeric_hint_is_malformed_cursor)
{
    auto const result = parse(req(std::format(R"JSON(, "marker": "{},abc")JSON", kHex1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountLinesSpec, marker_with_trailing_garbage_after_hint_is_malformed_cursor)
{
    auto const result = parse(req(std::format(R"JSON(, "marker": "{},7x")JSON", kHex1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

// --- deprecated fields ------------------------------------------------------

TEST(AccountLinesSpec, deprecated_fields_do_not_fail_the_request)
{
    auto const result = parse(req(R"JSON(, "ledger": 5, "peer_index": 3)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}
