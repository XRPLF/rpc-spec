/**
 * @file
 *  GTest coverage for the `account_tx` delegate filter, mirroring Clio's
 *  CustomValidators::delegateValidator wire contract.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_tx/Spec.hpp>
#include <rpcspec/handlers/account_tx/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using handlers::account_tx::DelegateFilter;

namespace {

constexpr auto kAccount = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kCounterparty = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

auto
parseAccountTx(std::string const& delegateJson)
{
    auto const json = std::format(R"JSON({{"account": "{}"{}}})JSON", kAccount, delegateJson);
    auto value = boost::json::parse(json);
    return handlers::account_tx::kInputSpecV1.parse(value);
}

}  // namespace

TEST(AccountTxDelegateSpec, AbsentDelegateLeavesFilterUnset)
{
    auto const result = parseAccountTx("");
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_FALSE(result->delegateFilter.has_value());
}

TEST(AccountTxDelegateSpec, ActorParses)
{
    auto const result = parseAccountTx(R"JSON(, "delegate": {"delegate_filter": "actor"})JSON");
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->delegateFilter.has_value());
    EXPECT_EQ(result->delegateFilter->delegateType, DelegateFilter::Role::Actor);
    EXPECT_FALSE(result->delegateFilter->counterParty.has_value());
}

TEST(AccountTxDelegateSpec, AuthorizerWithCounterPartyParses)
{
    auto const result = parseAccountTx(
        std::format(
            R"JSON(, "delegate": {{"delegate_filter": "authorizer", "counter_party": "{}"}})JSON",
            kCounterparty));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->delegateFilter.has_value());
    EXPECT_EQ(result->delegateFilter->delegateType, DelegateFilter::Role::Authorizer);
    ASSERT_TRUE(result->delegateFilter->counterParty.has_value());
    EXPECT_EQ(*result->delegateFilter->counterParty, kCounterparty);
}

TEST(AccountTxDelegateSpec, NotAnObjectFails)
{
    auto const result = parseAccountTx(R"JSON(, "delegate": "actor")JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "delegateNotObject");
}

TEST(AccountTxDelegateSpec, MissingDelegateFilterFails)
{
    auto const result = parseAccountTx(R"JSON(, "delegate": {})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Field 'delegate_filter' is required but missing.");
}

TEST(AccountTxDelegateSpec, UnknownDelegateFilterValueFails)
{
    auto const result = parseAccountTx(R"JSON(, "delegate": {"delegate_filter": "bogus"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(
        result.error().message, "Field 'delegate_filter' value must be 'actor' or 'authorizer'.");
}

TEST(AccountTxDelegateSpec, NonStringDelegateFilterFails)
{
    auto const result = parseAccountTx(R"JSON(, "delegate": {"delegate_filter": 1})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(
        result.error().message, "Field 'delegate_filter' value must be 'actor' or 'authorizer'.");
}

TEST(AccountTxDelegateSpec, MalformedCounterPartyFails)
{
    auto const result = parseAccountTx(
        R"JSON(, "delegate": {"delegate_filter": "actor", "counter_party": "not-an-account"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "Field 'counter_party' value must be a valid account.");
}
