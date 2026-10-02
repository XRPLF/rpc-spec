/**
 * @file
 *  GTest coverage for the `get_aggregate_price` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  `oracles` is guarded by a hand-written CustomModifier with several distinct
 *  error arms; `trim` and the two asset fields carry declarative constraints
 *  whose error codes are part of the wire contract.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/get_aggregate_price/Spec.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::get_aggregate_price;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

// A request with the two required assets plus a caller-supplied `oracles` blob.
std::string
withOracles(std::string const& oraclesJson)
{
    return std::format(
        R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "oracles": {}}})JSON", oraclesJson);
}

std::string
oneOracle()
{
    return std::format(R"JSON([{{"oracle_document_id": 1, "account": "{}"}}])JSON", kAcct1);
}

}  // namespace

TEST(GetAggregatePriceSpec, minimal_valid_request_parses)
{
    auto const result = parse(withOracles(oneOracle()));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_EQ(result->oracles.size(), 1u);
    EXPECT_EQ(result->oracles[0].documentId, 1u);
}

TEST(GetAggregatePriceSpec, base_asset_required)
{
    auto value = boost::json::parse(
        std::format(R"JSON({{"quote_asset": "XRP", "oracles": {}}})JSON", oneOracle()));
    auto const result = kInputSpec.parse(value);
    ASSERT_FALSE(result.has_value());
}

TEST(GetAggregatePriceSpec, oracles_required)
{
    auto const result = parse(R"JSON({"base_asset": "USD", "quote_asset": "XRP"})JSON");
    ASSERT_FALSE(result.has_value());
}

// --- oracles: kOraclesValidator arms ---------------------------------------

TEST(GetAggregatePriceSpec, oracles_not_array_is_oracle_malformed)
{
    auto const result = parse(withOracles(R"JSON("nope")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, oracles_empty_array_is_oracle_malformed)
{
    auto const result = parse(withOracles("[]"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, oracles_element_not_object_is_oracle_malformed)
{
    auto const result = parse(withOracles(R"JSON([1])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, oracles_missing_document_id_is_oracle_malformed)
{
    auto const result = parse(withOracles(std::format(R"JSON([{{"account": "{}"}}])JSON", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, oracles_missing_account_is_oracle_malformed)
{
    auto const result = parse(withOracles(R"JSON([{"oracle_document_id": 1}])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, oracles_document_id_wrong_type_is_invalid_params)
{
    // Neither uint32 nor string — rejected by the Type<uint32_t, std::string> gate with
    // RpcInvalidParams, matching xrpld and Clio 2.8.0 (only a *missing* id is malformed).
    for (auto const* docId : {"-1", "null", "2.3", "true", "{}", "[]", "4294967296"})
    {
        SCOPED_TRACE(docId);
        auto const result = parse(withOracles(
            std::format(
                R"JSON([{{"oracle_document_id": {}, "account": "{}"}}])JSON", docId, kAcct1)));
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    }
}

TEST(GetAggregatePriceSpec, oracles_document_id_numeric_string_is_accepted)
{
    auto const result = parse(withOracles(
        std::format(R"JSON([{{"oracle_document_id": "123", "account": "{}"}}])JSON", kAcct1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_EQ(result->oracles.size(), 1u);
    EXPECT_EQ(result->oracles[0].documentId, 123u);
}

TEST(GetAggregatePriceSpec, oracles_document_id_non_numeric_string_is_invalid_params)
{
    // The type gate passes (it *is* a string), then ToNumberModifier fails with
    // InvalidParams.
    auto const result = parse(withOracles(
        std::format(R"JSON([{{"oracle_document_id": "a", "account": "{}"}}])JSON", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(GetAggregatePriceSpec, oracles_malformed_account_is_invalid_params)
{
    auto const result =
        parse(withOracles(R"JSON([{"oracle_document_id": 1, "account": "notanaccount"}])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(GetAggregatePriceSpec, oracles_non_string_account_is_invalid_params)
{
    auto const result = parse(withOracles(R"JSON([{"oracle_document_id": 1, "account": 5}])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

// --- trim / time_threshold --------------------------------------------------

TEST(GetAggregatePriceSpec, trim_at_bounds_parses)
{
    for (auto const* trim : {"1", "25"})
    {
        auto value = boost::json::parse(
            std::format(
                R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "oracles": {}, "trim": {}}})JSON",
                oneOracle(),
                trim));
        auto const result = kInputSpec.parse(value);
        ASSERT_TRUE(result.has_value()) << "trim=" << trim << " msg: " << result.error().message;
        ASSERT_TRUE(result->trim.has_value());
    }
}

TEST(GetAggregatePriceSpec, trim_out_of_range_is_rejected)
{
    for (auto const* trim : {"0", "26"})
    {
        auto value = boost::json::parse(
            std::format(
                R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "oracles": {}, "trim": {}}})JSON",
                oneOracle(),
                trim));
        auto const result = kInputSpec.parse(value);
        ASSERT_FALSE(result.has_value()) << "trim=" << trim << " unexpectedly accepted";
    }
}

TEST(GetAggregatePriceSpec, time_threshold_parses)
{
    auto value = boost::json::parse(
        std::format(
            R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "oracles": {}, "time_threshold": 60}})JSON",
            oneOracle()));
    auto const result = kInputSpec.parse(value);
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->timeThreshold.has_value());
    EXPECT_EQ(*result->timeThreshold, 60u);
}
