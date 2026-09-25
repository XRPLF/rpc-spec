/**
 * @file
 *  GTest coverage for the `noripple_check` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  Two things worth pinning: `RoleGatewayConverter` collapses the `role` enum to
 *  a bool, and v1/v2 differ in how `transactions` coerces (`jsonBool` vs
 *  `jsonBoolStrict`) — a version-dependent split of the same shape as
 *  `gateway_balances`' hotwallet error codes.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/noripple_check/Spec.hpp>
#include <rpcspec/handlers/noripple_check/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::noripple_check;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

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
req(std::string const& extra = {})
{
    return std::format(R"JSON({{"account": "{}", "role": "user"{}}})JSON", kAcct1, extra);
}

}  // namespace

TEST(NoRippleCheckSpec, AccountAndRoleRequired)
{
    EXPECT_FALSE(parseV1(R"JSON({})JSON").has_value());
    EXPECT_FALSE(parseV1(std::format(R"JSON({{"account": "{}"}})JSON", kAcct1)).has_value());
    EXPECT_FALSE(parseV1(R"JSON({"role": "user"})JSON").has_value());
}

TEST(NoRippleCheckSpec, RoleUserIsNotGateway)
{
    auto const result = parseV1(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->roleGateway);
}

TEST(NoRippleCheckSpec, RoleGatewayIsGateway)
{
    auto const result =
        parseV1(std::format(R"JSON({{"account": "{}", "role": "gateway"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->roleGateway);
}

TEST(NoRippleCheckSpec, UnknownRoleIsRejectedWithCustomMessage)
{
    auto const result =
        parseV1(std::format(R"JSON({{"account": "{}", "role": "bogus"}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "role field is invalid");
}

TEST(NoRippleCheckSpec, NonStringRoleIsRejectedWithCustomMessage)
{
    // The withCustomError wraps the whole oneOf, so a type failure reads the same.
    auto const result = parseV1(std::format(R"JSON({{"account": "{}", "role": 5}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "role field is invalid");
}

TEST(NoRippleCheckSpec, RoleIsCaseSensitive)
{
    auto const result =
        parseV1(std::format(R"JSON({{"account": "{}", "role": "Gateway"}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

// --- limit ------------------------------------------------------------------

TEST(NoRippleCheckSpec, LimitDefaults)
{
    auto const result = parseV1(req());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, kLimitDefault);
}

TEST(NoRippleCheckSpec, LimitAboveMaxIsClamped)
{
    auto const result = parseV1(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitMax);
}

TEST(NoRippleCheckSpec, LimitBelowMinIsRejected)
{
    // min() runs before clamp(), so 0 is an error rather than being clamped up.
    auto const result = parseV1(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
}

TEST(NoRippleCheckSpec, LimitInRangeIsPreserved)
{
    auto const result = parseV1(req(R"JSON(, "limit": 42)JSON"));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, 42u);
}

// --- transactions: the v1/v2 split -----------------------------------------

TEST(NoRippleCheckSpec, TransactionsBoolAcceptedOnBothVersions)
{
    auto const v1 = parseV1(req(R"JSON(, "transactions": true)JSON"));
    ASSERT_TRUE(v1.has_value()) << "msg: " << v1.error().message;
    EXPECT_TRUE(v1->transactions);

    auto const v2 = parseV2(req(R"JSON(, "transactions": true)JSON"));
    ASSERT_TRUE(v2.has_value()) << "msg: " << v2.error().message;
    EXPECT_TRUE(v2->transactions);
}

TEST(NoRippleCheckSpec, V1TransactionsCoercesNonBool)
{
    // jsonBool is lenient: a non-bool is coerced rather than rejected.
    auto const result = parseV1(req(R"JSON(, "transactions": 1)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->transactions);
}

TEST(NoRippleCheckSpec, V2TransactionsRejectsNonBool)
{
    // jsonBoolStrict requires an actual JSON bool.
    auto const result = parseV2(req(R"JSON(, "transactions": 1)JSON"));
    ASSERT_FALSE(result.has_value()) << "v2 unexpectedly coerced a non-bool";
}
