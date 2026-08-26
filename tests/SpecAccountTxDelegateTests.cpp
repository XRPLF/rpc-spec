/** @file
 *  GTest coverage for the `account_tx` delegate filter, mirroring Clio's
 *  CustomValidators::delegateValidator wire contract.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_tx/Spec.hpp>
#include <rpcspec/handlers/account_tx/Types.hpp>

#include <string>

using namespace rpc::spec;
using handlers::account_tx::DelegateFilter;

namespace {

constexpr char const* kACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr char const* kCOUNTERPARTY = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";

auto
parseAccountTx(std::string const& delegateJson)
{
    auto const json =
        std::string{R"JSON({"account": ")JSON"} + kACCOUNT + R"JSON(")JSON" + delegateJson + "}";
    auto value = boost::json::parse(json);
    return handlers::account_tx::kInputSpecV1.parse(value);
}

}  // namespace

TEST(AccountTxDelegateSpec, AbsentDelegateLeavesFilterUnset)
{
    auto const r = parseAccountTx("");
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    EXPECT_FALSE(r->delegateFilter.has_value());
}

TEST(AccountTxDelegateSpec, ActorParses)
{
    auto const r = parseAccountTx(R"JSON(, "delegate": {"delegate_filter": "actor"})JSON");
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    ASSERT_TRUE(r->delegateFilter.has_value());
    EXPECT_EQ(r->delegateFilter->delegateType, DelegateFilter::Role::Actor);
    EXPECT_FALSE(r->delegateFilter->counterParty.has_value());
}

TEST(AccountTxDelegateSpec, AuthorizerWithCounterPartyParses)
{
    auto const r = parseAccountTx(
        std::string{
            R"JSON(, "delegate": {"delegate_filter": "authorizer", "counter_party": ")JSON"} +
        kCOUNTERPARTY + R"JSON("})JSON");
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    ASSERT_TRUE(r->delegateFilter.has_value());
    EXPECT_EQ(r->delegateFilter->delegateType, DelegateFilter::Role::Authorizer);
    ASSERT_TRUE(r->delegateFilter->counterParty.has_value());
    EXPECT_EQ(*r->delegateFilter->counterParty, kCOUNTERPARTY);
}

TEST(AccountTxDelegateSpec, NotAnObjectFails)
{
    auto const r = parseAccountTx(R"JSON(, "delegate": "actor")JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "delegateNotObject");
}

TEST(AccountTxDelegateSpec, MissingDelegateFilterFails)
{
    auto const r = parseAccountTx(R"JSON(, "delegate": {})JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Field 'delegate_filter' is required but missing.");
}

TEST(AccountTxDelegateSpec, UnknownDelegateFilterValueFails)
{
    auto const r = parseAccountTx(R"JSON(, "delegate": {"delegate_filter": "bogus"})JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Field 'delegate_filter' value must be 'actor' or 'authorizer'.");
}

TEST(AccountTxDelegateSpec, NonStringDelegateFilterFails)
{
    auto const r = parseAccountTx(R"JSON(, "delegate": {"delegate_filter": 1})JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Field 'delegate_filter' value must be 'actor' or 'authorizer'.");
}

TEST(AccountTxDelegateSpec, MalformedCounterPartyFails)
{
    auto const r = parseAccountTx(
        R"JSON(, "delegate": {"delegate_filter": "actor", "counter_party": "not-an-account"})JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcActMalformed);
    EXPECT_EQ(r.error().message, "Field 'counter_party' value must be a valid account.");
}
