/**
 * @file
 *  GTest coverage for the `account_objects` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/handlers/account_objects/Spec.hpp>
#include <rpcspec/handlers/account_objects/Types.hpp>

#include <xrpl_mock.hpp>

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_objects;

namespace {

constexpr char const* kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV1.parse(value);
}

}  // namespace

TEST(AccountObjectsSpec, SponsoredAbsentLeavesFilterUnset)
{
    auto const r = parse(R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"})JSON");
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->sponsored.has_value());
}

TEST(AccountObjectsSpec, SponsoredTrue)
{
    auto const r =
        parse(R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": true})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->sponsored.has_value());
    EXPECT_TRUE(static_cast<bool>(*r->sponsored));
}

TEST(AccountObjectsSpec, SponsoredFalseIsDistinctFromAbsent)
{
    auto const r =
        parse(R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": false})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->sponsored.has_value());
    EXPECT_FALSE(static_cast<bool>(*r->sponsored));
}

TEST(AccountObjectsSpec, SponsoredRejectsNonBool)
{
    // xrpld requires a strict JSON bool here.
    for (auto const* body : {
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": "true"})JSON",
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": 1})JSON",
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": null})JSON",
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": {}})JSON",
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": []})JSON",
         })
        EXPECT_FALSE(parse(body).has_value()) << "should have been rejected: " << body;
}

TEST(AccountObjectsSpec, TypeAcceptsSponsorship)
{
    auto const r = parse(
        R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "type": "sponsorship"})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->type.has_value());
    EXPECT_EQ(*r->type, xrpl::ltSPONSORSHIP);
}

TEST(AccountObjectsSpec, TypeRejectsUnknownAndChainScopedTypes)
{
    // `amendments` is chain-scoped, not account-owned, so it is not a valid filter.
    for (auto const* body : {
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "type": "sponsorships"})JSON",
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "type": "amendments"})JSON",
         })
        EXPECT_FALSE(parse(body).has_value()) << "should have been rejected: " << body;
}
