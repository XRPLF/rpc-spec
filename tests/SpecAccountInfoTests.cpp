/**
 * @file
 *  GTest coverage for the `account_info` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  xrpld reads `ident` when `account` is absent, decodes the account only after
 *  it has resolved the ledger, and checks a v2 `signer_lists` in its handler. The
 *  Clio arm is pinned by SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/AccountTypes.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_info/Spec.hpp>
#include <rpcspec/handlers/account_info/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <initializer_list>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_info;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

auto
parse(std::string const& json, unsigned apiVersion = 1)
{
    auto value = boost::json::parse(json);
    return apiVersion == 1 ? kInputSpecV1.parse(value) : kInputSpecV2.parse(value);
}

std::string
req(std::string const& extra = {})
{
    return std::format(R"JSON({{"account": "{}"{}}})JSON", kAcct1, extra);
}

}  // namespace

TEST(AccountInfoSpec, account_parses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->account.has_value());
    EXPECT_TRUE(result->account->has_value());
    EXPECT_FALSE(result->ident.has_value());
}

TEST(AccountInfoSpec, ident_is_read_when_account_is_absent)
{
    auto const result = parse(std::format(R"JSON({{"ident": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->account.has_value());
    ASSERT_TRUE(result->ident.has_value());
    EXPECT_TRUE(result->ident->has_value());
}

TEST(AccountInfoSpec, ident_is_ignored_when_account_is_present)
{
    for (auto const* ident : {"5", R"JSON("llIIOO")JSON"})
    {
        auto const result = parse(req(std::format(R"JSON(, "ident": {})JSON", ident)));
        ASSERT_TRUE(result.has_value()) << "ident=" << ident;
        EXPECT_TRUE(result->account.has_value()) << "ident=" << ident;
        EXPECT_FALSE(result->ident.has_value()) << "ident=" << ident;
    }
}

TEST(AccountInfoSpec, neither_is_a_missing_account)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'account'.");
}

TEST(AccountInfoSpec, non_string_is_reported_under_its_own_key)
{
    for (auto const* key : {"account", "ident"})
    {
        for (auto const* bad : {"1", "1.1", "true", "null", "{}", "[]"})
        {
            auto const result = parse(std::format(R"JSON({{"{}": {}}})JSON", key, bad));
            ASSERT_FALSE(result.has_value()) << key << "=" << bad;
            EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << key << "=" << bad;
            EXPECT_EQ(result.error().message, std::format("Invalid field '{}'.", key))
                << key << "=" << bad;
        }
    }
}

TEST(AccountInfoSpec, malformed_account_is_left_to_the_handler)
{
    auto const fromAccount = parse(R"JSON({"account": "llIIOO"})JSON");
    ASSERT_TRUE(fromAccount.has_value());
    ASSERT_TRUE(fromAccount->account.has_value());
    ASSERT_FALSE(fromAccount->account->has_value());
    EXPECT_EQ(fromAccount->account->error(), AccountError::Malformed);

    auto const fromIdent = parse(R"JSON({"ident": "llIIOO"})JSON");
    ASSERT_TRUE(fromIdent.has_value());
    ASSERT_TRUE(fromIdent->ident.has_value());
    ASSERT_FALSE(fromIdent->ident->has_value());
    EXPECT_EQ(fromIdent->ident->error(), AccountError::Malformed);
}

// --- signer_lists / queue -----------------------------------------------------

TEST(AccountInfoSpec, signer_lists_is_coerced_on_every_api_version)
{
    for (unsigned const apiVersion : {1u, 2u})
    {
        auto const result = parse(req(R"JSON(, "signer_lists": "yes")JSON"), apiVersion);
        ASSERT_TRUE(result.has_value()) << "api_version=" << apiVersion;
        EXPECT_TRUE(result->signerLists) << "api_version=" << apiVersion;
    }
}

TEST(AccountInfoSpec, queue_is_coerced_to_bool)
{
    for (auto const* truthy : {"true", "1", R"JSON("false")JSON"})
    {
        auto const result = parse(req(std::format(R"JSON(, "queue": {})JSON", truthy)));
        ASSERT_TRUE(result.has_value()) << "queue=" << truthy;
        EXPECT_TRUE(result->queue) << "queue=" << truthy;
    }
    for (auto const* falsy : {"false", "0", "null"})
    {
        auto const result = parse(req(std::format(R"JSON(, "queue": {})JSON", falsy)));
        ASSERT_TRUE(result.has_value()) << "queue=" << falsy;
        EXPECT_FALSE(result->queue) << "queue=" << falsy;
    }
}

// --- deprecated fields ------------------------------------------------------

TEST(AccountInfoSpec, deprecated_fields_do_not_warn)
{
    auto const request =
        boost::json::parse(req(R"JSON(, "ident": "x", "ledger": 5, "strict": true)JSON"));
    EXPECT_TRUE(kInputSpecV1.check(request).empty());
    EXPECT_TRUE(kInputSpecV2.check(request).empty());
}
