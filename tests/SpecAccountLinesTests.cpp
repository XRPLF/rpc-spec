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

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_lines;

namespace {

constexpr char const* kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr char const* kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr char const* kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV1.parse(value);
}

std::string
req(std::string const& extra = {})
{
    return std::string{R"JSON({"account": ")JSON"} + kAcct1 + R"JSON(")JSON" + extra + "}";
}

}  // namespace

TEST(AccountLinesSpec, AccountRequired)
{
    EXPECT_FALSE(parse(R"JSON({})JSON").has_value());
}

TEST(AccountLinesSpec, MinimalRequestParses)
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

TEST(AccountLinesSpec, MalformedAccountIsActMalformed)
{
    auto const result = parse(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
}

TEST(AccountLinesSpec, NonStringAccountIsAlsoActMalformed)
{
    auto const result = parse(R"JSON({"account": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
}

TEST(AccountLinesSpec, PeerParses)
{
    auto const result = parse(req(std::string{R"JSON(, "peer": ")JSON"} + kAcct2 + R"JSON(")JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->peer.has_value());
}

TEST(AccountLinesSpec, MalformedPeerIsActMalformed)
{
    auto const result = parse(req(R"JSON(, "peer": "notanaccount")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
}

// --- AsBoolConverter --------------------------------------------------------

TEST(AccountLinesSpec, IgnoreDefaultBoolParses)
{
    auto const result = parse(req(R"JSON(, "ignore_default": true)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->ignoreDefault);
}

TEST(AccountLinesSpec, IgnoreDefaultNonBoolIsRejected)
{
    for (auto const* bad : {"1", R"("true")", "{}"})
    {
        auto const result = parse(req(std::string{R"JSON(, "ignore_default": )JSON"} + bad));
        ASSERT_FALSE(result.has_value()) << "ignore_default=" << bad << " unexpectedly accepted";
    }
}

// --- limit ------------------------------------------------------------------

TEST(AccountLinesSpec, LimitBelowClampFloorIsRaised)
{
    // min(1) admits it, then clamp(10, 400) raises it to the floor.
    auto const result = parse(req(R"JSON(, "limit": 5)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitMin);
}

TEST(AccountLinesSpec, LimitAboveMaxIsClamped)
{
    auto const result = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, kLimitMax);
}

TEST(AccountLinesSpec, LimitZeroIsRejected)
{
    auto const result = parse(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
}

// --- accountMarker ----------------------------------------------------------

TEST(AccountLinesSpec, WellFormedMarkerParses)
{
    auto const result =
        parse(req(std::string{R"JSON(, "marker": ")JSON"} + kHex1 + R"JSON(,7")JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
}

TEST(AccountLinesSpec, NonStringMarkerNamesTheField)
{
    auto const result = parse(req(R"JSON(, "marker": 5)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerNotString");
}

TEST(AccountLinesSpec, MarkerWithoutCommaIsMalformedCursor)
{
    auto const result =
        parse(req(std::string{R"JSON(, "marker": ")JSON"} + kHex1 + R"JSON(")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountLinesSpec, MarkerWithBadHexIsMalformedCursor)
{
    auto const result = parse(req(R"JSON(, "marker": "NOTHEX,7")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountLinesSpec, MarkerWithNonNumericHintIsMalformedCursor)
{
    auto const result =
        parse(req(std::string{R"JSON(, "marker": ")JSON"} + kHex1 + R"JSON(,abc")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountLinesSpec, MarkerWithTrailingGarbageAfterHintIsMalformedCursor)
{
    auto const result =
        parse(req(std::string{R"JSON(, "marker": ")JSON"} + kHex1 + R"JSON(,7x")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

// --- deprecated fields ------------------------------------------------------

TEST(AccountLinesSpec, DeprecatedFieldsDoNotFailTheRequest)
{
    auto const result = parse(req(R"JSON(, "ledger": 5, "peer_index": 3)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}
