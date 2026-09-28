/**
 * @file
 *  GTest coverage for the `deposit_authorized` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  `credentials` is validated by `hex256Array` and then converted by
 *  `CredentialsArrayConverter`, which documents a precondition that every
 *  element is already a well-formed uint256. These tests pin the validator arms
 *  that establish that precondition.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/deposit_authorized/Spec.hpp>
#include <rpcspec/handlers/deposit_authorized/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::deposit_authorized;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kHex2 = "0000000000000000000000000000000000000000000000000000000000000001";

std::string
base()
{
    return std::format(
        R"JSON("source_account": "{}", "destination_account": "{}")JSON", kAcct1, kAcct2);
}

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

auto
parseWithCredentials(std::string const& credentialsJson)
{
    return parse("{" + base() + R"JSON(, "credentials": )JSON" + credentialsJson + "}");
}

}  // namespace

TEST(DepositAuthorizedSpec, both_accounts_required)
{
    EXPECT_FALSE(parse(R"JSON({})JSON").has_value());
    EXPECT_FALSE(parse(std::format(R"JSON({{"source_account": "{}"}})JSON", kAcct1)).has_value());
    EXPECT_FALSE(
        parse(std::format(R"JSON({{"destination_account": "{}"}})JSON", kAcct2)).has_value());
}

TEST(DepositAuthorizedSpec, minimal_request_parses)
{
    auto const result = parse("{" + base() + "}");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->credentials.has_value());
}

TEST(DepositAuthorizedSpec, malformed_source_account_is_act_malformed)
{
    auto const result = parse(
        std::format(
            R"JSON({{"source_account": "notanaccount", "destination_account": "{}"}})JSON",
            kAcct2));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "source_accountMalformed");
}

TEST(DepositAuthorizedSpec, non_string_destination_account_is_invalid_params)
{
    auto const result = parse(
        std::format(R"JSON({{"source_account": "{}", "destination_account": 5}})JSON", kAcct1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "destination_accountNotString");
}

// --- credentials ------------------------------------------------------------

TEST(DepositAuthorizedSpec, credentials_array_parses)
{
    auto const result = parseWithCredentials(std::format(R"(["{}", "{}"])", kHex1, kHex2));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->credentials.has_value());
    EXPECT_EQ(result->credentials->size(), 2u);
}

TEST(DepositAuthorizedSpec, credentials_empty_array_parses)
{
    // hex256Array does not impose a size floor; emptiness is the handler's business.
    auto const result = parseWithCredentials("[]");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->credentials.has_value());
    EXPECT_TRUE(result->credentials->empty());
}

TEST(DepositAuthorizedSpec, credentials_duplicates_are_preserved)
{
    // The converter builds a vector, not a set: de-duplication is the handler's job.
    auto const result = parseWithCredentials(std::format(R"(["{}", "{}"])", kHex1, kHex1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->credentials.has_value());
    EXPECT_EQ(result->credentials->size(), 2u);
}

TEST(DepositAuthorizedSpec, credentials_not_array_is_bare_invalid_params)
{
    auto const result = parseWithCredentials(R"JSON("notanarray")JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    // Deliberately message-less, mirroring the old leading Type<array> check.
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}

TEST(DepositAuthorizedSpec, credentials_non_string_element_is_rejected)
{
    auto const result = parseWithCredentials(std::format(R"(["{}", 42])", kHex1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Item is not a valid uint256 type.");
}

TEST(DepositAuthorizedSpec, credentials_non_hex_element_is_rejected)
{
    auto const result = parseWithCredentials(R"JSON(["NOTHEX"])JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Item is not a valid uint256 type.");
}

TEST(DepositAuthorizedSpec, credentials_wrong_length_hex_is_rejected)
{
    // 63 characters: parseHex is strict about width.
    auto const result = parseWithCredentials(
        R"JSON(["1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B"])JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Item is not a valid uint256 type.");
}
