/**
 * @file
 *  GTest coverage for the `get_aggregate_price` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  xrpld checks the `oracles` array itself before the ledger, but each element only after it, so
 *  the first bad element's error is carried in `Input::oracles` rather than failing the parse.
 *  `trim` and `time_threshold` take a uint32 or a string holding one, read as
 *  `beast::lexicalCastChecked` reads it. The Clio arm is pinned in SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/get_aggregate_price/Spec.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <cstddef>
#include <format>
#include <string>
#include <utility>

using namespace rpc::spec;
using namespace rpc::spec::handlers::get_aggregate_price;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAccountZero = "rrrrrrrrrrrrrrrrrrrrrhoLvTp";
constexpr std::size_t kOraclesMaxCount = 200;

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

std::string
withField(std::string const& key, std::string const& valueJson)
{
    return std::format(
        R"JSON({{"base_asset": "USD", "quote_asset": "XRP", "oracles": {}, "{}": {}}})JSON",
        oneOracle(),
        key,
        valueJson);
}

void
expectError(std::string const& json, rpc::XrpldError code, std::string const& message = {})
{
    auto const result = parse(json);
    ASSERT_FALSE(result.has_value()) << json << " unexpectedly accepted";
    EXPECT_EQ(result.error(), code) << json;
    EXPECT_EQ(result.error().message, message) << json;
}

// The oracles of a request that parses, or what is wrong with them.
DeferredOracles
parseOracles(std::string const& oraclesJson)
{
    auto const result = parse(withOracles(oraclesJson));
    EXPECT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    return result.has_value() ? result->oracles : DeferredOracles{};
}

void
expectDeferred(std::string const& oraclesJson, DeferredError error)
{
    auto const oracles = parseOracles(oraclesJson);
    ASSERT_FALSE(oracles.has_value()) << oraclesJson << " unexpectedly accepted";
    EXPECT_EQ(oracles.error(), error) << oraclesJson;
}

}  // namespace

TEST(GetAggregatePriceSpec, minimal_valid_request_parses)
{
    auto const oracles = parseOracles(oneOracle());
    ASSERT_TRUE(oracles.has_value());
    ASSERT_EQ(oracles->size(), 1u);
    EXPECT_EQ((*oracles)[0].documentId, 1u);
}

TEST(GetAggregatePriceSpec, missing_fields_name_the_field)
{
    expectError(
        R"JSON({"base_asset": "USD", "quote_asset": "XRP"})JSON",
        rpc::XrpldError::RpcInvalidParams,
        "Missing field 'oracles'.");
    expectError(
        std::format(R"JSON({{"quote_asset": "XRP", "oracles": {}}})JSON", oneOracle()),
        rpc::XrpldError::RpcInvalidParams,
        "Missing field 'base_asset'.");
    expectError(
        std::format(R"JSON({{"base_asset": "USD", "oracles": {}}})JSON", oneOracle()),
        rpc::XrpldError::RpcInvalidParams,
        "Missing field 'quote_asset'.");
}

TEST(GetAggregatePriceSpec, missing_oracles_wins_over_missing_assets)
{
    expectError(R"JSON({})JSON", rpc::XrpldError::RpcInvalidParams, "Missing field 'oracles'.");
}

// --- oracles: the array, checked before the ledger ---------------------------

TEST(GetAggregatePriceSpec, oracles_that_is_not_a_non_empty_array_is_oracle_malformed)
{
    std::string tooMany = "[";
    for (std::size_t i = 0; i <= kOraclesMaxCount; ++i)
    {
        tooMany += std::format(
            R"JSON({}{{"oracle_document_id": {}, "account": "{}"}})JSON",
            i == 0 ? "" : ",",
            i,
            kAcct1);
    }
    tooMany += "]";

    for (std::string const& oracles : {
             std::string{R"JSON("nope")JSON"},
             std::string{R"JSON(null)JSON"},
             std::string{R"JSON({})JSON"},
             std::string{R"JSON([])JSON"},
             tooMany,
         })
    {
        expectError(withOracles(oracles), rpc::XrpldError::RpcOracleMalformed);
    }
}

// --- oracles: each element, deferred to the handler --------------------------

TEST(GetAggregatePriceSpec, element_without_both_keys_is_deferred_oracle_malformed)
{
    expectDeferred(R"JSON([1])JSON", DeferredError::Malformed);
    expectDeferred(R"JSON([null])JSON", DeferredError::Malformed);
    expectDeferred(
        std::format(R"JSON([{{"account": "{}"}}])JSON", kAcct1), DeferredError::Malformed);
    expectDeferred(R"JSON([{"oracle_document_id": 1}])JSON", DeferredError::Malformed);
}

TEST(GetAggregatePriceSpec, bad_document_id_is_deferred_invalid_params)
{
    for (auto const* docId :
         {"-1", "null", "2.3", "true", "{}", "[]", "4294967296", R"("a")", R"("-0")", R"("5.0")"})
    {
        expectDeferred(
            std::format(
                R"JSON([{{"oracle_document_id": {}, "account": "{}"}}])JSON", docId, kAcct1),
            DeferredError::Invalid);
    }
}

TEST(GetAggregatePriceSpec, document_id_string_is_read_as_xrpld_reads_it)
{
    for (auto const& [docId, expected] :
         {std::pair{"123", 123u}, std::pair{"+5", 5u}, std::pair{"007", 7u}})
    {
        auto const oracles = parseOracles(
            std::format(
                R"JSON([{{"oracle_document_id": "{}", "account": "{}"}}])JSON", docId, kAcct1));
        ASSERT_TRUE(oracles.has_value()) << docId;
        EXPECT_EQ((*oracles)[0].documentId, expected) << docId;
    }
}

TEST(GetAggregatePriceSpec, bad_account_is_deferred_invalid_params)
{
    for (std::string const& account : {
             std::string{R"JSON("notanaccount")JSON"},
             std::format(R"JSON("{}")JSON", kAccountZero),
             std::string{R"JSON(5)JSON"},
             std::string{R"JSON(null)JSON"},
             std::string{R"JSON([])JSON"},
             std::string{R"JSON({})JSON"},
         })
    {
        expectDeferred(
            std::format(R"JSON([{{"oracle_document_id": 1, "account": {}}}])JSON", account),
            DeferredError::Invalid);
    }
}

TEST(GetAggregatePriceSpec, first_bad_element_wins)
{
    expectDeferred(
        std::format(
            R"JSON([{{"oracle_document_id": 1, "account": "{}"}}, {{"oracle_document_id": "a", "account": "{}"}}, {{"account": "{}"}}])JSON",
            kAcct1,
            kAcct1,
            kAcct1),
        DeferredError::Invalid);
}

TEST(GetAggregatePriceSpec, duplicate_elements_are_kept_for_the_handler)
{
    auto const oracles = parseOracles(
        std::format(
            R"JSON([{{"oracle_document_id": 1, "account": "{}"}}, {{"oracle_document_id": 1, "account": "{}"}}])JSON",
            kAcct1,
            kAcct1));
    ASSERT_TRUE(oracles.has_value());
    EXPECT_EQ(oracles->size(), 2u);
}

// --- assets -----------------------------------------------------------------

TEST(GetAggregatePriceSpec, bad_asset_is_invalid_params)
{
    for (auto const* asset :
         {R"("")",
          "1",
          "null",
          R"("TOOLONGCURRENCY")",
          R"("0000000000000000000000005852500000000000")",
          R"("0000000000000000000000000000000000000001")"})
    {
        expectError(
            std::format(
                R"JSON({{"base_asset": {}, "quote_asset": "XRP", "oracles": {}}})JSON",
                asset,
                oneOracle()),
            rpc::XrpldError::RpcInvalidParams);
        expectError(
            std::format(
                R"JSON({{"base_asset": "USD", "quote_asset": {}, "oracles": {}}})JSON",
                asset,
                oneOracle()),
            rpc::XrpldError::RpcInvalidParams);
    }
}

TEST(GetAggregatePriceSpec, hex_asset_parses)
{
    auto const result = parse(
        std::format(
            R"JSON({{"base_asset": "0000000000000000000000005553440000000000", "quote_asset": "XRP", "oracles": {}}})JSON",
            oneOracle()));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
}

// --- trim / time_threshold --------------------------------------------------

TEST(GetAggregatePriceSpec, trim_at_bounds_parses)
{
    for (auto const* trim : {"1", "25", R"("1")", R"("+25")"})
    {
        auto const result = parse(withField("trim", trim));
        ASSERT_TRUE(result.has_value()) << "trim=" << trim << " msg: " << result.error().message;
        ASSERT_TRUE(result->trim.has_value());
    }
}

TEST(GetAggregatePriceSpec, bad_trim_is_invalid_params)
{
    for (auto const* trim : {"0", "26", R"("0")", R"("26")", R"("a")", R"("-0")", "-1", "null"})
        expectError(withField("trim", trim), rpc::XrpldError::RpcInvalidParams);
}

TEST(GetAggregatePriceSpec, time_threshold_parses)
{
    for (auto const* threshold : {"60", R"("60")", R"("+60")"})
    {
        auto const result = parse(withField("time_threshold", threshold));
        ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
        ASSERT_TRUE(result->timeThreshold.has_value());
        EXPECT_EQ(*result->timeThreshold, 60u);
    }
}

TEST(GetAggregatePriceSpec, bad_time_threshold_is_invalid_params)
{
    for (auto const* threshold : {R"("a")", R"("-0")", "-1", "2.5", "null", "true"})
        expectError(withField("time_threshold", threshold), rpc::XrpldError::RpcInvalidParams);
}
