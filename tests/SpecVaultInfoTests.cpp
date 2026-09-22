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

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::vault_info;

namespace {

constexpr char const* kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr char const* kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(VaultInfoSpec, EmptyRequestParses)
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

TEST(VaultInfoSpec, VaultIdParses)
{
    auto const result = parse(std::string{R"JSON({"vault_id": ")JSON"} + kHex1 + R"JSON("})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->vaultID.has_value());
}

TEST(VaultInfoSpec, OwnerAndSeqParse)
{
    auto const result =
        parse(std::string{R"JSON({"owner": ")JSON"} + kAcct1 + R"JSON(", "seq": 5})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->owner.has_value());
    ASSERT_TRUE(result->tnxSequence.has_value());
    EXPECT_EQ(*result->tnxSequence, 5u);
}

TEST(VaultInfoSpec, SeqZeroIsAcceptedBySpec)
{
    // xrpld rejects seq == 0 inside parseVault(), not in the spec; the spec's
    // job is only the type check.
    auto const result =
        parse(std::string{R"JSON({"owner": ")JSON"} + kAcct1 + R"JSON(", "seq": 0})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->tnxSequence.has_value());
    EXPECT_EQ(*result->tnxSequence, 0u);
}

// --- xrpld field errors -----------------------------------------------------

TEST(VaultInfoSpec, NonHexVaultIdIsInvalidParamsWithFieldMessage)
{
    auto const result = parse(R"JSON({"vault_id": "NOTHEX"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'vault_id', not hex string.");
}

TEST(VaultInfoSpec, NonStringVaultIdIsInvalidParamsWithFieldMessage)
{
    auto const result = parse(R"JSON({"vault_id": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'vault_id', not hex string.");
}

TEST(VaultInfoSpec, MalformedOwnerIsActMalformedWithFieldMessage)
{
    auto const result = parse(R"JSON({"owner": "notanaccount"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "Invalid field 'owner', not AccountID.");
}

TEST(VaultInfoSpec, NonStringOwnerIsActMalformedWithFieldMessage)
{
    auto const result = parse(R"JSON({"owner": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "Invalid field 'owner', not AccountID.");
}

TEST(VaultInfoSpec, NonIntegerSeqIsInvalidParamsWithFieldMessage)
{
    auto const result = parse(R"JSON({"seq": "5"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'seq', not a positive 32-bit integer.");
}

TEST(VaultInfoSpec, NegativeSeqIsInvalidParamsWithFieldMessage)
{
    auto const result = parse(R"JSON({"seq": -1})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'seq', not a positive 32-bit integer.");
}
