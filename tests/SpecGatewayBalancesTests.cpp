/**
 * @file
 *  GTest coverage for the `gateway_balances` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  The point of interest is `hotwallet`: v1 and v2 share an otherwise identical
 *  validator that differs *only* in the error code it reports
 *  (`RpcInvalidHotwallet` vs `RpcInvalidParams`). Both arms are exercised here so
 *  the pair cannot silently drift apart.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/handlers/gateway_balances/Spec.hpp>
#include <rpcspec/handlers/gateway_balances/Types.hpp>

#include <xrpl_mock.hpp>

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::gateway_balances;

namespace {

constexpr char const* kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr char const* kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

auto
parseV1(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV1.parse(value);
}

auto
parseV2(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV2.parse(value);
}

std::string
withHotWallet(std::string const& hotWalletJson)
{
    return std::string{R"JSON({"account": ")JSON"} + kAcct1 + R"JSON(", "hotwallet": )JSON" +
        hotWalletJson + "}";
}

}  // namespace

TEST(GatewayBalancesSpec, AccountRequired)
{
    auto const result = parseV1(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
}

TEST(GatewayBalancesSpec, AccountOnlyParsesWithNoHotWallets)
{
    auto const result = parseV1(std::string{R"JSON({"account": ")JSON"} + kAcct1 + R"JSON("})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->hotWallets.empty());
}

TEST(GatewayBalancesSpec, HotWalletSingleStringParses)
{
    auto const result = parseV1(withHotWallet(std::string{"\""} + kAcct2 + "\""));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->hotWallets.size(), 1u);
}

TEST(GatewayBalancesSpec, HotWalletArrayParses)
{
    auto const result =
        parseV1(withHotWallet(std::string{"[\""} + kAcct1 + "\", \"" + kAcct2 + "\"]"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->hotWallets.size(), 2u);
}

TEST(GatewayBalancesSpec, HotWalletArrayDeduplicates)
{
    // ValueType is std::set<AccountID>, so a repeated entry collapses.
    auto const result =
        parseV1(withHotWallet(std::string{"[\""} + kAcct1 + "\", \"" + kAcct1 + "\"]"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->hotWallets.size(), 1u);
}

TEST(GatewayBalancesSpec, HotWalletEmptyArrayParses)
{
    auto const result = parseV1(withHotWallet("[]"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->hotWallets.empty());
}

// --- the v1/v2 error-code split -------------------------------------------

TEST(GatewayBalancesSpec, V1HotWalletWrongTypeIsInvalidHotwallet)
{
    auto const result = parseV1(withHotWallet("123"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletNotStringOrArray");
}

TEST(GatewayBalancesSpec, V2HotWalletWrongTypeIsInvalidParams)
{
    auto const result = parseV2(withHotWallet("123"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "hotwalletNotStringOrArray");
}

TEST(GatewayBalancesSpec, V1HotWalletMalformedStringIsInvalidHotwallet)
{
    auto const result = parseV1(withHotWallet(R"JSON("notanaccount")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, V2HotWalletMalformedStringIsInvalidParams)
{
    auto const result = parseV2(withHotWallet(R"JSON("notanaccount")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, V1HotWalletMalformedArrayElementIsInvalidHotwallet)
{
    auto const result =
        parseV1(withHotWallet(std::string{"[\""} + kAcct1 + "\", \"notanaccount\"]"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, V2HotWalletMalformedArrayElementIsInvalidParams)
{
    auto const result =
        parseV2(withHotWallet(std::string{"[\""} + kAcct1 + "\", \"notanaccount\"]"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, HotWalletNonStringArrayElementIsRejected)
{
    auto const result = parseV1(withHotWallet(std::string{"[\""} + kAcct1 + "\", 42]"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}
