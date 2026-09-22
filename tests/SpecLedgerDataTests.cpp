/**
 * @file
 *  GTest coverage for the `ledger_data` typed spec — xrpld arm.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  `MarkerConverter` accepts two shapes (uint256 hex string, or a uint32 diff
 *  marker) and its failure message is server-conditional; the Clio arm lives in
 *  SpecClioHandlerErrorsTests.cpp. `LedgerEntryTypeConverter` maps the `type`
 *  string onto a LedgerEntryType and rejects unknown names.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/ledger_data/Spec.hpp>
#include <rpcspec/handlers/ledger_data/Types.hpp>

#include <xrpl_mock.hpp>

#include <cstdint>
#include <string>
#include <variant>

using namespace rpc::spec;
using namespace rpc::spec::handlers::ledger_data;

namespace {

constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(LedgerDataSpec, EmptyRequestParses)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->binary);
    EXPECT_FALSE(result->limit.has_value());
    EXPECT_FALSE(result->marker.has_value());
    EXPECT_FALSE(result->outOfOrder);
}

// --- limit ------------------------------------------------------------------

TEST(LedgerDataSpec, LimitIsNotUpperBoundedBySpec)
{
    // Deliberate: the effective cap depends on `binary`, so the handler resolves it.
    auto const result = parse(R"JSON({"limit": 100000})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, 100000u);
}

TEST(LedgerDataSpec, LimitZeroIsRejected)
{
    EXPECT_FALSE(parse(R"JSON({"limit": 0})JSON").has_value());
}

TEST(LedgerDataSpec, LimitBooleanIsRejected)
{
    // isIntegral() is true for JSON booleans in some backends; type<uint32_t> must not admit it.
    EXPECT_FALSE(parse(R"JSON({"limit": true})JSON").has_value());
}

// --- MarkerConverter --------------------------------------------------------

TEST(LedgerDataSpec, HexStringMarkerParsesAsUint256)
{
    auto const result = parse(std::string{R"JSON({"marker": ")JSON"} + kHex1 + R"JSON("})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_TRUE(std::holds_alternative<xrpl::uint256>(*result->marker));
}

TEST(LedgerDataSpec, Uint32MarkerParsesAsDiffMarker)
{
    auto const result = parse(R"JSON({"marker": 42})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    ASSERT_TRUE(std::holds_alternative<uint32_t>(*result->marker));
    EXPECT_EQ(std::get<uint32_t>(*result->marker), 42u);
}

TEST(LedgerDataSpec, NonHexStringMarkerIsMalformedField)
{
    auto const result = parse(R"JSON({"marker": "NOTHEX"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(LedgerDataSpec, OtherMarkerTypesReportMarkerNotString)
{
    // The xrpld arm carries an explicit token here; Clio's is message-less.
    for (auto const* bad : {"true", "{}", "[]", "-1"})
    {
        auto const result = parse(std::string{R"JSON({"marker": )JSON"} + bad + "}");
        ASSERT_FALSE(result.has_value()) << "marker=" << bad << " unexpectedly accepted";
        EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_EQ(result.error().message, "markerNotString") << "marker=" << bad;
    }
}

// --- LedgerEntryTypeConverter ----------------------------------------------

TEST(LedgerDataSpec, KnownTypeParses)
{
    auto const result = parse(R"JSON({"type": "account"})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_NE(result->type, xrpl::ltANY);
}

TEST(LedgerDataSpec, UnknownTypeIsInvalidField)
{
    auto const result = parse(R"JSON({"type": "bogus"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'type'.");
}

TEST(LedgerDataSpec, NonStringTypeIsExpectedFieldError)
{
    auto const result = parse(R"JSON({"type": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'type', not string.");
}

// --- strict bools -----------------------------------------------------------

TEST(LedgerDataSpec, BinaryAndOutOfOrderAcceptBools)
{
    auto const result = parse(R"JSON({"binary": true, "out_of_order": true})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(result->binary);
    EXPECT_TRUE(result->outOfOrder);
}

TEST(LedgerDataSpec, BinaryRejectsNonBool)
{
    EXPECT_FALSE(parse(R"JSON({"binary": 1})JSON").has_value());
}

TEST(LedgerDataSpec, OutOfOrderRejectsNonBool)
{
    EXPECT_FALSE(parse(R"JSON({"out_of_order": "true"})JSON").has_value());
}

TEST(LedgerDataSpec, DeprecatedLedgerFieldDoesNotFailTheRequest)
{
    auto const result = parse(R"JSON({"ledger": 5})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}
