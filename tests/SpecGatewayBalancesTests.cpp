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
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/gateway_balances/Spec.hpp>
#include <rpcspec/handlers/gateway_balances/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::gateway_balances;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

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
    return std::format(R"JSON({{"account": "{}", "hotwallet": {}}})JSON", kAcct1, hotWalletJson);
}

}  // namespace

TEST(GatewayBalancesSpec, account_required)
{
    auto const result = parseV1(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
}

TEST(GatewayBalancesSpec, account_only_parses_with_no_hot_wallets)
{
    auto const result = parseV1(std::format(R"JSON({{"account": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->hotWallets.empty());
}

TEST(GatewayBalancesSpec, hot_wallet_single_string_parses)
{
    auto const result = parseV1(withHotWallet(std::format(R"("{}")", kAcct2)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->hotWallets.size(), 1u);
}

TEST(GatewayBalancesSpec, hot_wallet_array_parses)
{
    auto const result = parseV1(withHotWallet(std::format(R"(["{}", "{}"])", kAcct1, kAcct2)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->hotWallets.size(), 2u);
}

TEST(GatewayBalancesSpec, hot_wallet_array_deduplicates)
{
    // ValueType is std::set<AccountID>, so a repeated entry collapses.
    auto const result = parseV1(withHotWallet(std::format(R"(["{}", "{}"])", kAcct1, kAcct1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->hotWallets.size(), 1u);
}

TEST(GatewayBalancesSpec, hot_wallet_empty_array_parses)
{
    auto const result = parseV1(withHotWallet("[]"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->hotWallets.empty());
}

// --- the v1/v2 error-code split -------------------------------------------

TEST(GatewayBalancesSpec, v1_hot_wallet_wrong_type_is_invalid_hotwallet)
{
    auto const result = parseV1(withHotWallet("123"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletNotStringOrArray");
}

TEST(GatewayBalancesSpec, v2_hot_wallet_wrong_type_is_invalid_params)
{
    auto const result = parseV2(withHotWallet("123"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "hotwalletNotStringOrArray");
}

TEST(GatewayBalancesSpec, v1_hot_wallet_malformed_string_is_invalid_hotwallet)
{
    auto const result = parseV1(withHotWallet(R"JSON("notanaccount")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, v2_hot_wallet_malformed_string_is_invalid_params)
{
    auto const result = parseV2(withHotWallet(R"JSON("notanaccount")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, v1_hot_wallet_malformed_array_element_is_invalid_hotwallet)
{
    auto const result = parseV1(withHotWallet(std::format(R"(["{}", "notanaccount"])", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, v2_hot_wallet_malformed_array_element_is_invalid_params)
{
    auto const result = parseV2(withHotWallet(std::format(R"(["{}", "notanaccount"])", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}

TEST(GatewayBalancesSpec, hot_wallet_non_string_array_element_is_rejected)
{
    auto const result = parseV1(withHotWallet(std::format(R"(["{}", 42])", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidHotwallet);
    EXPECT_EQ(result.error().message, "hotwalletMalformed");
}
