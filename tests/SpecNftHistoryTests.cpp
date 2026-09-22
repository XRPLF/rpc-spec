/**
 * @file
 *  GTest coverage for the `nft_history` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  Two converters worth pinning: `Int32BoundConverter` treats -1 as "unset"
 *  (a sentinel, not a value), and `MarkerConverter` reads `ledger`/`seq`
 *  children on the strength of a preceding `section(...)` validator.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/nft_history/Spec.hpp>
#include <rpcspec/handlers/nft_history/Types.hpp>

#include <cstdint>
#include <limits>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::nft_history;

namespace {

constexpr auto kNftId = "00080000B4F4AFC5FBCBD76873F18006173D2193467D3EE70000099B00000000";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV1.parse(value);
}

std::string
req(std::string const& extra = {})
{
    return std::string{R"JSON({"nft_id": ")JSON"} + kNftId + R"JSON(")JSON" + extra + "}";
}

}  // namespace

TEST(NftHistorySpec, NftIdRequired)
{
    EXPECT_FALSE(parse(R"JSON({})JSON").has_value());
}

TEST(NftHistorySpec, MinimalRequestParses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->ledgerIndexMin.has_value());
    EXPECT_FALSE(result->ledgerIndexMax.has_value());
    EXPECT_FALSE(result->limit.has_value());
    EXPECT_FALSE(result->marker.has_value());
}

TEST(NftHistorySpec, MalformedNftIdIsRejected)
{
    EXPECT_FALSE(parse(R"JSON({"nft_id": "NOTHEX"})JSON").has_value());
}

// --- Int32BoundConverter: the -1 sentinel ----------------------------------

TEST(NftHistorySpec, LedgerIndexMinMinusOneMeansUnset)
{
    auto const result = parse(req(R"JSON(, "ledger_index_min": -1)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->ledgerIndexMin.has_value());
}

TEST(NftHistorySpec, LedgerIndexMaxMinusOneMeansUnset)
{
    auto const result = parse(req(R"JSON(, "ledger_index_max": -1)JSON"));
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->ledgerIndexMax.has_value());
}

TEST(NftHistorySpec, LedgerIndexBoundsAreKept)
{
    auto const result = parse(req(R"JSON(, "ledger_index_min": 10, "ledger_index_max": 20)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->ledgerIndexMin.has_value());
    ASSERT_TRUE(result->ledgerIndexMax.has_value());
    EXPECT_EQ(*result->ledgerIndexMin, 10);
    EXPECT_EQ(*result->ledgerIndexMax, 20);
}

TEST(NftHistorySpec, LedgerIndexAboveInt32MaxIsClamped)
{
    auto const result = parse(req(R"JSON(, "ledger_index_max": 5000000000)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->ledgerIndexMax.has_value());
    EXPECT_EQ(*result->ledgerIndexMax, std::numeric_limits<int32_t>::max());
}

TEST(NftHistorySpec, LedgerIndexNonIntegerIsRejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "ledger_index_min": "10")JSON")).has_value());
}

// --- limit ------------------------------------------------------------------

TEST(NftHistorySpec, LimitInRangeIsKept)
{
    auto const result = parse(req(R"JSON(, "limit": 42)JSON"));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, 42u);
}

TEST(NftHistorySpec, LimitAboveMaxIsClamped)
{
    auto const result = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, kLimitMax);
}

TEST(NftHistorySpec, LimitZeroIsRejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "limit": 0)JSON")).has_value());
}

// --- MarkerConverter + section() -------------------------------------------

TEST(NftHistorySpec, WellFormedMarkerParses)
{
    auto const result = parse(req(R"JSON(, "marker": {"ledger": 7, "seq": 9})JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(result->marker->ledger, 7u);
    EXPECT_EQ(result->marker->seq, 9u);
}

TEST(NftHistorySpec, NonObjectMarkerReportsInvalidMarker)
{
    for (auto const* bad : {"5", R"("x")", "true", "[]"})
    {
        auto const result = parse(req(std::string{R"JSON(, "marker": )JSON"} + bad));
        ASSERT_FALSE(result.has_value()) << "marker=" << bad << " unexpectedly accepted";
        EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_EQ(result.error().message, "invalidMarker") << "marker=" << bad;
    }
}

TEST(NftHistorySpec, MarkerMissingLedgerIsRejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"seq": 9})JSON")).has_value());
}

TEST(NftHistorySpec, MarkerMissingSeqIsRejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"ledger": 7})JSON")).has_value());
}

TEST(NftHistorySpec, MarkerWithWrongChildTypeIsRejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"ledger": "7", "seq": 9})JSON")).has_value());
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"ledger": 7, "seq": -1})JSON")).has_value());
}

// --- binary / forward: strict on both versions ------------------------------

TEST(NftHistorySpec, BinaryAndForwardAcceptBools)
{
    auto const result = parse(req(R"JSON(, "binary": true, "forward": true)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(static_cast<bool>(result->binary));
    EXPECT_TRUE(static_cast<bool>(result->forward));
}

TEST(NftHistorySpec, BinaryRejectsNonBoolOnBothVersions)
{
    // Unlike account_tx, nft_history applies jsonBoolStrict on v1 too.
    auto v1 = boost::json::parse(req(R"JSON(, "binary": 1)JSON"));
    EXPECT_FALSE(kInputSpecV1.parse(v1).has_value());

    auto v2 = boost::json::parse(req(R"JSON(, "binary": 1)JSON"));
    EXPECT_FALSE(kInputSpecV2.parse(v2).has_value());
}

TEST(NftHistorySpec, ForwardRejectsNonBool)
{
    EXPECT_FALSE(parse(req(R"JSON(, "forward": "true")JSON")).has_value());
}
