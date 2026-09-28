/**
 * @file
 *  GTest coverage for the `vault_info` typed spec — xrpld arm.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  Every field error in this handler is server-conditional: xrpld uses
 *  field-specific rippled codes, Clio collapses them onto
 *  ClioError::RpcMalformedRequest. The Clio arm is in
 *  SpecClioHandlerErrorsTests.cpp; keeping both pinned is what stops the pair
 *  drifting, which is the failure mode this handler has already had once.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/vault_info/Spec.hpp>
#include <rpcspec/handlers/vault_info/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::vault_info;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(VaultInfoSpec, empty_request_parses)
{
    // No field is `required`; the owner/seq-vs-vault_id combination rule lives
    // in the handler, not the spec.
    auto const result = parse(R"JSON({})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->vaultID.has_value());
    EXPECT_FALSE(result->owner.has_value());
    EXPECT_FALSE(result->tnxSequence.has_value());
}

TEST(VaultInfoSpec, vault_id_parses)
{
    auto const result = parse(std::format(R"JSON({{"vault_id": "{}"}})JSON", kHex1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->vaultID.has_value());
}

TEST(VaultInfoSpec, owner_and_seq_parse)
{
    auto const result = parse(std::format(R"JSON({{"owner": "{}", "seq": 5}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->owner.has_value());
    ASSERT_TRUE(result->tnxSequence.has_value());
    EXPECT_EQ(*result->tnxSequence, 5u);
}

TEST(VaultInfoSpec, seq_zero_is_accepted_by_spec)
{
    // xrpld rejects seq == 0 inside parseVault(), not in the spec; the spec's
    // job is only the type check.
    auto const result = parse(std::format(R"JSON({{"owner": "{}", "seq": 0}})JSON", kAcct1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->tnxSequence.has_value());
    EXPECT_EQ(*result->tnxSequence, 0u);
}

// --- xrpld field errors -----------------------------------------------------

TEST(VaultInfoSpec, non_hex_vault_id_is_invalid_params_with_field_message)
{
    auto const result = parse(R"JSON({"vault_id": "NOTHEX"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'vault_id', not hex string.");
}

TEST(VaultInfoSpec, non_string_vault_id_is_invalid_params_with_field_message)
{
    auto const result = parse(R"JSON({"vault_id": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'vault_id', not hex string.");
}

TEST(VaultInfoSpec, malformed_owner_is_act_malformed_with_field_message)
{
    auto const result = parse(R"JSON({"owner": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "Invalid field 'owner', not AccountID.");
}

TEST(VaultInfoSpec, non_string_owner_is_act_malformed_with_field_message)
{
    auto const result = parse(R"JSON({"owner": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "Invalid field 'owner', not AccountID.");
}

TEST(VaultInfoSpec, non_integer_seq_is_invalid_params_with_field_message)
{
    auto const result = parse(R"JSON({"seq": "5"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'seq', not a positive 32-bit integer.");
}

TEST(VaultInfoSpec, negative_seq_is_invalid_params_with_field_message)
{
    auto const result = parse(R"JSON({"seq": -1})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'seq', not a positive 32-bit integer.");
}
