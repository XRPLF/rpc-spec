/**
 * @file
 *  GTest coverage for the `amm_info` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  NOTE on the string form of `asset`/`asset2`: the string arm of
 *  `IssueConverter::parse` (and `kStringIssueValidator`) is driven entirely by
 *  `xrpl::issueFromJson`, which the test stub implements as "accept anything"
 *  (see tests/stubs/xrpl_mock.hpp) whereas real libxrpl throws
 *  `std::runtime_error` on malformed input. Asserting the rejection behaviour
 *  here would pin mock behaviour that contradicts production, so the string arm
 *  is deliberately left to Clio's handler tests, which link real libxrpl. What
 *  *is* asserted below is everything the mock models faithfully: the type gate
 *  and the object arm.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/amm_info/Spec.hpp>
#include <rpcspec/handlers/amm_info/Types.hpp>

#include <xrpl_mock.hpp>

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::amm_info;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

// `asset` as an object, with `asset2` held at a fixed valid value.
std::string
withAsset(std::string const& assetJson)
{
    return std::string{R"JSON({"asset": )JSON"} + assetJson +
        R"JSON(, "asset2": {"currency": "XRP"}})JSON";
}

}  // namespace

TEST(AmmInfoSpec, EmptyRequestParses)
{
    // Neither asset nor asset2 is `required`; both default to noIssue().
    auto const result = parse(R"JSON({})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AmmInfoSpec, ObjectAssetsParse)
{
    auto const result = parse(
        std::string{R"JSON({"asset": {"currency": "USD", "issuer": ")JSON"} + kAcct1 +
        R"JSON("}, "asset2": {"currency": "XRP"}})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AmmInfoSpec, XrpObjectAssetYieldsXrpIssue)
{
    auto const result = parse(
        std::string{R"JSON({"asset": {"currency": "XRP"}, "asset2": {"currency": "USD",)JSON"} +
        R"JSON( "issuer": ")JSON" + kAcct1 + R"JSON("}})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->issue1, xrpl::xrpIssue());
}

// --- the type gate ----------------------------------------------------------

TEST(AmmInfoSpec, AssetNeitherStringNorObjectIsIssueMalformed)
{
    for (auto const* bad : {"123", "true", "[]", "null"})
    {
        auto const result = parse(withAsset(bad));
        ASSERT_FALSE(result.has_value()) << "asset=" << bad << " unexpectedly accepted";
        EXPECT_EQ(result.error(), rpc::RippledError::RpcIssueMalformed) << "asset=" << bad;
    }
}

TEST(AmmInfoSpec, Asset2NeitherStringNorObjectIsIssueMalformed)
{
    auto const result = parse(R"JSON({"asset": {"currency": "XRP"}, "asset2": 123})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcIssueMalformed);
}

// --- the object arm ---------------------------------------------------------

TEST(AmmInfoSpec, ObjectAssetBadCurrencyIsIssueMalformed)
{
    auto const result = parse(withAsset(R"JSON({"currency": "TOOLONGCURRENCY"})JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcIssueMalformed);
}

TEST(AmmInfoSpec, ObjectAssetBadIssuerIsIssueMalformed)
{
    auto const result =
        parse(withAsset(R"JSON({"currency": "USD", "issuer": "notanaccount"})JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcIssueMalformed);
}

TEST(AmmInfoSpec, ObjectAssetMissingCurrencyIsIssueMalformed)
{
    auto const result =
        parse(withAsset(std::string{R"JSON({"issuer": ")JSON"} + kAcct1 + R"JSON("})JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcIssueMalformed);
}

TEST(AmmInfoSpec, ObjectAssetNonXrpMissingIssuerIsIssueMalformed)
{
    auto const result = parse(withAsset(R"JSON({"currency": "USD"})JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcIssueMalformed);
}

// --- account fields ---------------------------------------------------------

TEST(AmmInfoSpec, AccountAndAmmAccountParse)
{
    auto const result = parse(
        std::string{R"JSON({"account": ")JSON"} + kAcct1 + R"JSON(", "amm_account": ")JSON" +
        kAcct2 + R"JSON("})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->accountID.has_value());
    ASSERT_TRUE(result->ammAccount.has_value());
}

TEST(AmmInfoSpec, MalformedAccountIsActMalformed)
{
    auto const result = parse(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
}

TEST(AmmInfoSpec, MalformedAmmAccountIsActMalformed)
{
    auto const result = parse(R"JSON({"amm_account": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
}
