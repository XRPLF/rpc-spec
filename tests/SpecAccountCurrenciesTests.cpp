/**
 * @file
 *  GTest coverage for the `account_currencies` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  xrpld reads `ident` when `account` is absent, and decodes the account only
 *  after it has resolved the ledger, so a malformed one reaches the handler as
 *  an `AccountError`. The Clio arm is pinned by SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_currencies/Spec.hpp>
#include <rpcspec/handlers/account_currencies/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <initializer_list>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_currencies;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(AccountCurrenciesSpec, account_parses)
{
    auto const result = parse(std::format(R"JSON({{"account": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpec, ident_is_read_when_account_is_absent)
{
    auto const result = parse(std::format(R"JSON({{"ident": "{}"}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpec, ident_is_ignored_when_account_is_present)
{
    auto const result = parse(std::format(R"JSON({{"account": "{}", "ident": 5}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(AccountCurrenciesSpec, neither_is_a_missing_account)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'account'.");
}

TEST(AccountCurrenciesSpec, non_string_is_reported_under_its_own_key)
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

TEST(AccountCurrenciesSpec, malformed_account_is_left_to_the_handler)
{
    for (auto const* key : {"account", "ident"})
    {
        auto const result = parse(std::format(R"JSON({{"{}": "llIIOO"}})JSON", key));
        ASSERT_TRUE(result.has_value())
            << "error: " << result.error().error << " msg: " << result.error().message;
        ASSERT_FALSE(result->account.has_value()) << key;
        EXPECT_EQ(result->account.error(), AccountError::Malformed) << key;
    }
}

TEST(AccountCurrenciesSpec, deprecated_fields_do_not_warn)
{
    auto const request = boost::json::parse(
        std::format(R"JSON({{"account": "{}", "account_index": 1, "strict": true}})JSON", kAcct1));
    EXPECT_TRUE(kInputSpec.check(request).empty());
}
