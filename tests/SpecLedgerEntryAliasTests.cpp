/**
 * @file
 *  GTest coverage for the `ledger_entry` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD and RPCSPEC_IS_CLIO.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/ledger_entry/Spec.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <string>
#include <variant>

using namespace rpc::spec;
using namespace rpc::spec::handlers::ledger_entry;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kHex64 = "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(LedgerEntrySpec, AccountAliasLocator)
{
    auto const result = parse(R"JSON({"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->accountRoot.has_value());
    EXPECT_EQ(*result->accountRoot, *rpc::spec::detail::accountFromStringStrict(kAcct1));
}

TEST(LedgerEntrySpec, StateAliasObjectLocator)
{
    auto const result = parse(R"JSON({"state": {
        "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"],
        "currency": "USD"
    }})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->rippleStateAccount.has_value());
    ASSERT_TRUE(std::holds_alternative<RippleStateEntry>(*result->rippleStateAccount));
    auto const& entry = std::get<RippleStateEntry>(*result->rippleStateAccount);
    EXPECT_EQ(entry.accounts[0], *rpc::spec::detail::accountFromStringStrict(kAcct1));
    EXPECT_EQ(entry.accounts[1], *rpc::spec::detail::accountFromStringStrict(kAcct2));
    EXPECT_EQ(entry.currency, rpc::spec::detail::currencyFromValidated("USD"));
}

TEST(LedgerEntrySpec, StateHexLocators)
{
    for (auto const* name : {"state", "ripple_state"})
    {
        SCOPED_TRACE(name);
        auto const result = parse(std::string{"{\""} + name + "\":\"" + kHex64 + "\"}");
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->rippleStateAccount.has_value());
        ASSERT_TRUE(std::holds_alternative<xrpl::uint256>(*result->rippleStateAccount));
        xrpl::uint256 expected;
        ASSERT_TRUE(expected.parseHex(kHex64));
        EXPECT_EQ(std::get<xrpl::uint256>(*result->rippleStateAccount), expected);
    }
}

TEST(LedgerEntrySpec, RejectsBothAccountSpellings)
{
    auto const result = parse(R"JSON({
        "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
        "account_root": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        (rpc::Status{rpc::XrpldError::RpcInvalidParams, "Too many fields provided."}));
}

TEST(LedgerEntrySpec, RejectsBothStateSpellings)
{
    auto const result = parse(R"JSON({
        "state": {"accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"], "currency": "USD"},
        "ripple_state": {"accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"], "currency": "USD"}
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error(),
        (rpc::Status{rpc::XrpldError::RpcInvalidParams, "Too many fields provided."}));
}

TEST(LedgerEntrySpec, InvalidAliasValuesAreRejected)
{
    for (auto const* json :
         {R"JSON({"account": 123})JSON",
          R"JSON({"account": "invalid"})JSON",
          R"JSON({"state": 123})JSON",
          R"JSON({"state": "invalid"})JSON",
          R"JSON({"state": {}})JSON"})
    {
        SCOPED_TRACE(json);
        EXPECT_FALSE(parse(json).has_value());
    }
}
