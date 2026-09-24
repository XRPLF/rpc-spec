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

#include <Backend.hpp>

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

TEST(GetAggregatePriceSpec, MinimalValidRequestParses)
{
    auto const result = parse(withOracles(oneOracle()));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_EQ(result->oracles.size(), 1u);
    EXPECT_EQ(result->oracles[0].documentId, 1u);
}

TEST(GetAggregatePriceSpec, BaseAssetRequired)
{
    auto value = boost::json::parse(
        std::format(R"JSON({{"quote_asset": "XRP", "oracles": {}}})JSON", oneOracle()));
    auto const result = kInputSpec.parse(value);
    ASSERT_FALSE(result.has_value());
}

TEST(GetAggregatePriceSpec, OraclesRequired)
{
    auto const result = parse(R"JSON({"base_asset": "USD", "quote_asset": "XRP"})JSON");
    ASSERT_FALSE(result.has_value());
}

// --- oracles: kOraclesValidator arms ---------------------------------------

TEST(GetAggregatePriceSpec, OraclesNotArrayIsOracleMalformed)
{
    auto const result = parse(withOracles(R"JSON("nope")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, OraclesEmptyArrayIsOracleMalformed)
{
    auto const result = parse(withOracles("[]"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, OraclesElementNotObjectIsOracleMalformed)
{
    auto const result = parse(withOracles(R"JSON([1])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, OraclesMissingDocumentIdIsOracleMalformed)
{
    auto const result = parse(withOracles(std::format(R"JSON([{{"account": "{}"}}])JSON", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, OraclesMissingAccountIsOracleMalformed)
{
    auto const result = parse(withOracles(R"JSON([{"oracle_document_id": 1}])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, OraclesDocumentIdWrongTypeIsOracleMalformed)
{
    // Neither uint32 nor string — rejected by the Type<uint32_t, std::string> gate.
    auto const result = parse(withOracles(
        std::format(R"JSON([{{"oracle_document_id": {{}}, "account": "{}"}}])JSON", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcOracleMalformed);
}

TEST(GetAggregatePriceSpec, OraclesDocumentIdNumericStringIsAccepted)
{
    auto const result = parse(withOracles(
        std::format(R"JSON([{{"oracle_document_id": "123", "account": "{}"}}])JSON", kAcct1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_EQ(result->oracles.size(), 1u);
    EXPECT_EQ(result->oracles[0].documentId, 123u);
}

TEST(GetAggregatePriceSpec, OraclesDocumentIdNonNumericStringIsInvalidParams)
{
    // Deliberate asymmetry: the type gate passes (it *is* a string), then
    // ToNumberModifier fails, and its InvalidParams is propagated verbatim
    // rather than being remapped to RpcOracleMalformed.
    auto const result = parse(withOracles(
        std::format(R"JSON([{{"oracle_document_id": "a", "account": "{}"}}])JSON", kAcct1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(GetAggregatePriceSpec, OraclesMalformedAccountIsInvalidParams)
{
    auto const result =
        parse(withOracles(R"JSON([{"oracle_document_id": 1, "account": "notanaccount"}])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(GetAggregatePriceSpec, OraclesNonStringAccountIsInvalidParams)
{
    auto const result = parse(withOracles(R"JSON([{"oracle_document_id": 1, "account": 5}])JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

// --- trim / time_threshold --------------------------------------------------

TEST(GetAggregatePriceSpec, TrimAtBoundsParses)
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

TEST(GetAggregatePriceSpec, TrimOutOfRangeIsRejected)
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

TEST(GetAggregatePriceSpec, TimeThresholdParses)
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
