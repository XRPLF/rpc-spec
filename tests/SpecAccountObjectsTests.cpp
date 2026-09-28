/**
 * @file
 *  GTest coverage for the `account_objects` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/handlers/account_objects/Spec.hpp>
#include <rpcspec/handlers/account_objects/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_objects;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV1.parse(value);
}

}  // namespace

TEST(AccountObjectsSpec, sponsored_absent_leaves_filter_unset)
{
    auto const result = parse(R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"})JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->sponsored.has_value());
}

TEST(AccountObjectsSpec, sponsored_true)
{
    auto const result =
        parse(R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": true})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->sponsored.has_value());
    EXPECT_TRUE(static_cast<bool>(*result->sponsored));
}

TEST(AccountObjectsSpec, sponsored_false_is_distinct_from_absent)
{
    auto const result =
        parse(R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsored": false})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->sponsored.has_value());
    EXPECT_FALSE(static_cast<bool>(*result->sponsored));
}

TEST(AccountObjectsSpec, sponsored_rejects_non_bool)
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

TEST(AccountObjectsSpec, type_accepts_sponsorship)
{
    auto const result = parse(
        R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "type": "sponsorship"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->type.has_value());
    EXPECT_EQ(*result->type, xrpl::ltSPONSORSHIP);
}

TEST(AccountObjectsSpec, type_rejects_unknown_and_chain_scoped_types)
{
    // `amendments` is chain-scoped, not account-owned, so it is not a valid filter.
    for (auto const* body : {
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "type": "sponsorships"})JSON",
             R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "type": "amendments"})JSON",
         })
        EXPECT_FALSE(parse(body).has_value()) << "should have been rejected: " << body;
}
