/**
 * @file
 *  GTest coverage for the `ledger_index` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  The whole spec is one optional `date` field: `timeFormat(kDateFormat)` gates
 *  it and `DateConverter` then parses it, relying on that gate having run.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/handlers/ledger_index/Spec.hpp>
#include <rpcspec/handlers/ledger_index/Types.hpp>

#include <xrpl_mock.hpp>

#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::ledger_index;

namespace {

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(LedgerIndexSpec, DateAbsentLeavesUnset)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->date.has_value());
}

TEST(LedgerIndexSpec, WellFormedDateParses)
{
    auto const result = parse(R"JSON({"date": "2024-01-15T12:30:45Z"})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->date.has_value());
}

TEST(LedgerIndexSpec, DistinctDatesProduceDistinctTimePoints)
{
    auto const a = parse(R"JSON({"date": "2024-01-15T12:30:45Z"})JSON");
    auto const b = parse(R"JSON({"date": "2024-01-15T12:30:46Z"})JSON");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    ASSERT_TRUE(a->date.has_value());
    ASSERT_TRUE(b->date.has_value());
    EXPECT_LT(*a->date, *b->date);
}

TEST(LedgerIndexSpec, NonStringDateIsInvalidParams)
{
    for (auto const* bad : {"123", "true", "{}", "[]"})
    {
        auto const result = parse(std::string{R"JSON({"date": )JSON"} + bad + "}");
        ASSERT_FALSE(result.has_value()) << "date=" << bad << " unexpectedly accepted";
        EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams) << "date=" << bad;
    }
}

TEST(LedgerIndexSpec, MisformattedDateIsInvalidParams)
{
    // Wrong separator, missing Z, plain date, and outright garbage.
    for (auto const* bad :
         {"2024-01-15 12:30:45Z", "2024-01-15T12:30:45", "2024-01-15", "notadate", ""})
    {
        auto const result = parse(std::string{R"JSON({"date": ")JSON"} + bad + R"JSON("})JSON");
        ASSERT_FALSE(result.has_value()) << "date=" << bad << " unexpectedly accepted";
        EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams) << "date=" << bad;
    }
}
