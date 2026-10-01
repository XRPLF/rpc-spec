/** @file */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/ledger_entry/Spec.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <array>
#include <string>
#include <variant>

using namespace rpc::spec::handlers::ledger_entry;

namespace {

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(LedgerEntrySpecClio, account_alias_populates_account_root)
{
    auto const result = parse(R"JSON({"account":"rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"})JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->accountRoot.has_value());
}

TEST(LedgerEntrySpecClio, state_alias_accepts_object_and_hex)
{
    auto const object = parse(R"JSON({
        "state": {
            "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"],
            "currency": "USD"
        }
    })JSON");
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(object->rippleStateAccount.has_value());
    EXPECT_TRUE(std::holds_alternative<RippleStateEntry>(*object->rippleStateAccount));

    auto const hex = parse(
        R"JSON({"state":"ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(hex.has_value());
    ASSERT_TRUE(hex->rippleStateAccount.has_value());
    EXPECT_TRUE(std::holds_alternative<xrpl::uint256>(*hex->rippleStateAccount));
}

TEST(LedgerEntrySpecClio, conflicting_aliases_are_rejected)
{
    static constexpr std::array kCases{
        R"JSON({
            "account_root": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"
        })JSON",
        R"JSON({
            "ripple_state": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789",
            "state": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"
        })JSON"};
    for (auto const* json : kCases)
    {
        SCOPED_TRACE(json);
        auto const result = parse(json);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(
            result.error(),
            (rpc::Status{rpc::XrpldError::RpcInvalidParams, "Too many fields provided."}));
    }
}
