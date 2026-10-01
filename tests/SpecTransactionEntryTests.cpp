/**
 * @file
 *  GTest coverage for the `transaction_entry` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  xrpld checks `tx_hash` only after it has resolved the ledger, so its spec
 *  never rejects `tx_hash`: a missing or malformed hash reaches the handler as
 *  a `TxHashError`. The Clio arm, which rejects both, is pinned by
 *  SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/transaction_entry/Spec.hpp>
#include <rpcspec/handlers/transaction_entry/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::transaction_entry;

namespace {

constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(TransactionEntrySpec, valid_hash_parses)
{
    auto const result = parse(std::format(R"JSON({{"tx_hash": "{}"}})JSON", kHex1));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->txHash.has_value());

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHex1));
    EXPECT_EQ(*result->txHash, expected);
}

TEST(TransactionEntrySpec, missing_hash_is_left_to_the_handler)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_FALSE(result->txHash.has_value());
    EXPECT_EQ(result->txHash.error(), TxHashError::Missing);
}

TEST(TransactionEntrySpec, malformed_hash_is_left_to_the_handler)
{
    for (auto const* hash :
         {R"JSON("DEADBEEF")JSON", R"JSON("")JSON", "42", "true", "null", "[]", "{}"})
    {
        auto const result = parse(std::format(R"JSON({{"tx_hash": {}}})JSON", hash));
        ASSERT_TRUE(result.has_value())
            << hash << " error: " << result.error().error << " msg: " << result.error().message;
        ASSERT_FALSE(result->txHash.has_value()) << hash;
        EXPECT_EQ(result->txHash.error(), TxHashError::Malformed) << hash;
    }
}

TEST(TransactionEntrySpec, bad_ledger_selector_still_fails)
{
    auto const result = parse(R"JSON({"ledger_index": "potato"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}
