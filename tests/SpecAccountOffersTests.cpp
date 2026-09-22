/**
 * @file
 *  GTest coverage for the `account_offers` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  `AccountMarkerStrConverter` re-implements the shared `accountMarker`
 *  validator's parse inside a converter (it returns the string rather than
 *  merely accepting it), so its arms are pinned independently here.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_offers/Spec.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_offers;

namespace {

constexpr char const* kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
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

std::string
withMarker(std::string const& marker)
{
    return req(std::string{R"JSON(, "marker": ")JSON"} + marker + R"JSON(")JSON");
}

}  // namespace

TEST(AccountOffersSpec, AccountRequired)
{
    EXPECT_FALSE(parse(R"JSON({})JSON").has_value());
}

TEST(AccountOffersSpec, MinimalRequestParses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
    EXPECT_FALSE(result->marker.has_value());
}

TEST(AccountOffersSpec, MalformedAccountIsActMalformed)
{
    auto const result = parse(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "accountMalformed");
}

TEST(AccountOffersSpec, NonStringAccountIsInvalidParams)
{
    // The shared accountId converter distinguishes wrong-type from unparseable.
    auto const result = parse(R"JSON({"account": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "accountNotString");
}

// --- limit ------------------------------------------------------------------

TEST(AccountOffersSpec, LimitBelowClampFloorIsRaised)
{
    auto const result = parse(req(R"JSON(, "limit": 5)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitMin);
}

TEST(AccountOffersSpec, LimitAboveMaxIsClamped)
{
    auto const result = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, kLimitMax);
}

TEST(AccountOffersSpec, LimitZeroIsRejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "limit": 0)JSON")).has_value());
}

// --- AccountMarkerStrConverter ---------------------------------------------

TEST(AccountOffersSpec, WellFormedMarkerRoundTripsAsString)
{
    auto const marker = std::string{kHex1} + ",7";
    auto const result = parse(withMarker(marker));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(*result->marker, marker);
}

TEST(AccountOffersSpec, MarkerHintZeroIsAccepted)
{
    auto const result = parse(withMarker(std::string{kHex1} + ",0"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AccountOffersSpec, NonStringMarkerNamesTheField)
{
    auto const result = parse(req(R"JSON(, "marker": 5)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerNotString");
}

TEST(AccountOffersSpec, MarkerWithoutCommaIsMalformedCursor)
{
    auto const result = parse(withMarker(kHex1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, MarkerWithBadHexIsMalformedCursor)
{
    auto const result = parse(withMarker("NOTHEX,7"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, MarkerWithEmptyHintIsMalformedCursor)
{
    auto const result = parse(withMarker(std::string{kHex1} + ","));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, MarkerWithTrailingGarbageAfterHintIsMalformedCursor)
{
    auto const result = parse(withMarker(std::string{kHex1} + ",7x"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, MarkerWithNegativeHintIsMalformedCursor)
{
    // from_chars into uint64_t rejects a leading '-'.
    auto const result = parse(withMarker(std::string{kHex1} + ",-1"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, DeprecatedFieldsDoNotFailTheRequest)
{
    auto const result = parse(req(R"JSON(, "ledger": 5, "strict": true)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}
