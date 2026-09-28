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

#include <Backend.hpp>  // IWYU pragma: keep

#include <cstdint>
#include <format>
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
    return std::format(R"JSON({{"nft_id": "{}"{}}})JSON", kNftId, extra);
}

}  // namespace

TEST(NftHistorySpec, nft_id_required)
{
    EXPECT_FALSE(parse(R"JSON({})JSON").has_value());
}

TEST(NftHistorySpec, minimal_request_parses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->ledgerIndexMin.has_value());
    EXPECT_FALSE(result->ledgerIndexMax.has_value());
    EXPECT_FALSE(result->limit.has_value());
    EXPECT_FALSE(result->marker.has_value());
}

TEST(NftHistorySpec, malformed_nft_id_is_rejected)
{
    EXPECT_FALSE(parse(R"JSON({"nft_id": "NOTHEX"})JSON").has_value());
}

// --- Int32BoundConverter: the -1 sentinel ----------------------------------

TEST(NftHistorySpec, ledger_index_min_minus_one_means_unset)
{
    auto const result = parse(req(R"JSON(, "ledger_index_min": -1)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->ledgerIndexMin.has_value());
}

TEST(NftHistorySpec, ledger_index_max_minus_one_means_unset)
{
    auto const result = parse(req(R"JSON(, "ledger_index_max": -1)JSON"));
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->ledgerIndexMax.has_value());
}

TEST(NftHistorySpec, ledger_index_bounds_are_kept)
{
    auto const result = parse(req(R"JSON(, "ledger_index_min": 10, "ledger_index_max": 20)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->ledgerIndexMin.has_value());
    ASSERT_TRUE(result->ledgerIndexMax.has_value());
    EXPECT_EQ(*result->ledgerIndexMin, 10);
    EXPECT_EQ(*result->ledgerIndexMax, 20);
}

TEST(NftHistorySpec, ledger_index_above_int32_max_is_clamped)
{
    auto const result = parse(req(R"JSON(, "ledger_index_max": 5000000000)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->ledgerIndexMax.has_value());
    EXPECT_EQ(*result->ledgerIndexMax, std::numeric_limits<int32_t>::max());
}

TEST(NftHistorySpec, ledger_index_non_integer_is_rejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "ledger_index_min": "10")JSON")).has_value());
}

// --- limit ------------------------------------------------------------------

TEST(NftHistorySpec, limit_in_range_is_kept)
{
    auto const result = parse(req(R"JSON(, "limit": 42)JSON"));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, 42u);
}

TEST(NftHistorySpec, limit_above_max_is_clamped)
{
    auto const result = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, kLimitMax);
}

TEST(NftHistorySpec, limit_zero_is_rejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "limit": 0)JSON")).has_value());
}

// --- MarkerConverter + section() -------------------------------------------

TEST(NftHistorySpec, well_formed_marker_parses)
{
    auto const result = parse(req(R"JSON(, "marker": {"ledger": 7, "seq": 9})JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(result->marker->ledger, 7u);
    EXPECT_EQ(result->marker->seq, 9u);
}

TEST(NftHistorySpec, non_object_marker_reports_invalid_marker)
{
    for (auto const* bad : {"5", R"("x")", "true", "[]"})
    {
        auto const result = parse(req(std::format(R"JSON(, "marker": {})JSON", bad)));
        ASSERT_FALSE(result.has_value()) << "marker=" << bad << " unexpectedly accepted";
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_EQ(result.error().message, "invalidMarker") << "marker=" << bad;
    }
}

TEST(NftHistorySpec, marker_missing_ledger_is_rejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"seq": 9})JSON")).has_value());
}

TEST(NftHistorySpec, marker_missing_seq_is_rejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"ledger": 7})JSON")).has_value());
}

TEST(NftHistorySpec, marker_with_wrong_child_type_is_rejected)
{
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"ledger": "7", "seq": 9})JSON")).has_value());
    EXPECT_FALSE(parse(req(R"JSON(, "marker": {"ledger": 7, "seq": -1})JSON")).has_value());
}

// --- binary / forward: strict on both versions ------------------------------

TEST(NftHistorySpec, binary_and_forward_accept_bools)
{
    auto const result = parse(req(R"JSON(, "binary": true, "forward": true)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_TRUE(static_cast<bool>(result->binary));
    EXPECT_TRUE(static_cast<bool>(result->forward));
}

TEST(NftHistorySpec, binary_rejects_non_bool_on_both_versions)
{
    // Unlike account_tx, nft_history applies jsonBoolStrict on v1 too.
    auto v1 = boost::json::parse(req(R"JSON(, "binary": 1)JSON"));
    EXPECT_FALSE(kInputSpecV1.parse(v1).has_value());

    auto v2 = boost::json::parse(req(R"JSON(, "binary": 1)JSON"));
    EXPECT_FALSE(kInputSpecV2.parse(v2).has_value());
}

TEST(NftHistorySpec, forward_rejects_non_bool)
{
    EXPECT_FALSE(parse(req(R"JSON(, "forward": "true")JSON")).has_value());
}
