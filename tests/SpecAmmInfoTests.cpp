/**
 * @file
 *  GTest coverage for the `amm_info` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  On xrpld every field but the ledger is deferred: a value that is not an asset or an account
 *  parses, as `DeferredError::Malformed` / `DeferredError::Malformed`, for the handler to report
 * after it has resolved the ledger. The asset rules are libxrpl's `assetFromJson`. The Clio arm is
 *  pinned in SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/AssetTypes.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/handlers/amm_info/Spec.hpp>
#include <rpcspec/handlers/amm_info/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <expected>
#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::amm_info;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kAccountZero = "rrrrrrrrrrrrrrrrrrrrrhoLvTp";
constexpr auto kAccountOne = "rrrrrrrrrrrrrrrrrrrrBZbvji";
constexpr auto kPublicKeyHex = "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020";
constexpr auto kMptId = "00000001A407AF5856CCF3C42619DAA925813FC955C72983";

Input
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    auto result = kInputSpec.parse(value);
    EXPECT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    return result.value_or(Input{});
}

DeferredAsset
parseAsset(std::string const& assetJson)
{
    return parse(std::format(R"JSON({{"asset": {}}})JSON", assetJson)).asset1;
}

}  // namespace

TEST(AmmInfoSpec, absent_fields_are_missing)
{
    auto const input = parse(R"JSON({})JSON");
    EXPECT_EQ(input.asset1, DeferredAsset{std::unexpected{DeferredError::Missing}});
    EXPECT_EQ(input.asset2, DeferredAsset{std::unexpected{DeferredError::Missing}});
    EXPECT_EQ(input.accountID, DeferredAccountId{std::unexpected{DeferredError::Missing}});
    EXPECT_EQ(input.ammAccount, DeferredAccountId{std::unexpected{DeferredError::Missing}});
}

// --- assets -----------------------------------------------------------------

TEST(AmmInfoSpec, issue_and_xrp_assets_parse)
{
    auto const input = parse(
        std::format(
            R"JSON({{"asset": {{"currency": "USD", "issuer": "{}"}}, "asset2": {{"currency": "XRP"}}}})JSON",
            kAcct1));
    ASSERT_TRUE(input.asset1.has_value());
    EXPECT_TRUE(input.asset1->holds<xrpl::Issue>());
    EXPECT_EQ(input.asset2, DeferredAsset{xrpl::xrpIssue()});
}

TEST(AmmInfoSpec, xrp_with_null_issuer_parses)
{
    EXPECT_EQ(
        parseAsset(R"JSON({"currency": "XRP", "issuer": null})JSON"),
        DeferredAsset{xrpl::xrpIssue()});
}

TEST(AmmInfoSpec, mpt_asset_parses)
{
    auto const asset = parseAsset(std::format(R"JSON({{"mpt_issuance_id": "{}"}})JSON", kMptId));
    ASSERT_TRUE(asset.has_value());
    EXPECT_TRUE(asset->holds<xrpl::MPTIssue>());
}

TEST(AmmInfoSpec, a_value_that_is_not_an_asset_is_deferred_as_malformed)
{
    for (
        std::string const& bad : {
            std::string{R"JSON(123)JSON"},
            std::string{R"JSON(true)JSON"},
            std::string{R"JSON([])JSON"},
            std::string{R"JSON(null)JSON"},
            std::string{R"JSON("XRP")JSON"},
            std::string{R"JSON({})JSON"},
            std::format(R"JSON({{"issuer": "{}"}})JSON", kAcct1),
            std::string{R"JSON({"currency": 1})JSON"},
            std::string{R"JSON({"currency": "TOOLONGCURRENCY"})JSON"},
            std::format(
                R"JSON({{"currency": "0000000000000000000000005852500000000000", "issuer": "{}"}})JSON",
                kAcct1),
            std::format(
                R"JSON({{"currency": "0000000000000000000000000000000000000001", "issuer": "{}"}})JSON",
                kAcct1),
            std::format(R"JSON({{"currency": "XRP", "issuer": "{}"}})JSON", kAcct1),
            std::string{R"JSON({"currency": "USD"})JSON"},
            std::string{R"JSON({"currency": "USD", "issuer": 1})JSON"},
            std::string{R"JSON({"currency": "USD", "issuer": "notanaccount"})JSON"},
            std::string{
                R"JSON({"currency": "USD", "issuer": "B5F762798A53D543A014CAF8B297CFF8F2F937E8"})JSON"},
            std::format(R"JSON({{"currency": "USD", "issuer": "{}"}})JSON", kAccountZero),
            std::format(R"JSON({{"currency": "USD", "issuer": "{}"}})JSON", kAccountOne),
            std::format(
                R"JSON({{"currency": "USD", "issuer": "{}", "mpt_issuance_id": "{}"}})JSON",
                kAcct1,
                kMptId),
            std::format(R"JSON({{"mpt_issuance_id": "{}", "issuer": "{}"}})JSON", kMptId, kAcct1),
            std::string{R"JSON({"mpt_issuance_id": 1})JSON"},
            std::string{R"JSON({"mpt_issuance_id": "NOTHEX"})JSON"},
        })
    {
        EXPECT_EQ(parseAsset(bad), DeferredAsset{std::unexpected{DeferredError::Malformed}})
            << "asset=" << bad;
    }
}

TEST(AmmInfoSpec, asset2_is_deferred_like_asset)
{
    auto const input = parse(R"JSON({"asset2": 123})JSON");
    EXPECT_EQ(input.asset2, DeferredAsset{std::unexpected{DeferredError::Malformed}});
}

// --- accounts ---------------------------------------------------------------

TEST(AmmInfoSpec, account_and_amm_account_parse)
{
    auto const input =
        parse(std::format(R"JSON({{"account": "{}", "amm_account": "{}"}})JSON", kAcct1, kAcct2));
    EXPECT_TRUE(input.accountID.has_value());
    EXPECT_TRUE(input.ammAccount.has_value());
}

TEST(AmmInfoSpec, a_value_that_is_not_a_base58_account_is_deferred_as_malformed)
{
    for (std::string const& bad : {
             std::string{R"JSON("notanaccount")JSON"},
             std::format(R"JSON("{}")JSON", kPublicKeyHex),
             std::string{R"JSON(42)JSON"},
             std::string{R"JSON(null)JSON"},
             std::string{R"JSON({})JSON"},
         })
    {
        auto const input =
            parse(std::format(R"JSON({{"account": {}, "amm_account": {}}})JSON", bad, bad));
        EXPECT_EQ(input.accountID, DeferredAccountId{std::unexpected{DeferredError::Malformed}})
            << "account=" << bad;
        EXPECT_EQ(input.ammAccount, DeferredAccountId{std::unexpected{DeferredError::Malformed}})
            << "amm_account=" << bad;
    }
}
