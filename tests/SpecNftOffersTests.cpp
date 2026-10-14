/**
 * @file
 *  GTest coverage for the `nft_buy_offers` / `nft_sell_offers` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  xrpld does not clamp `limit` in the spec (the handler clamps by role), and
 *  reports a malformed `marker` without a message. The Clio arm is pinned by
 *  SpecClioHandlerErrorsTests.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/nft_offers_common/Spec.hpp>
#include <rpcspec/handlers/nft_offers_common/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <format>
#include <initializer_list>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::nft_offers_common;

namespace {

constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kHex2 = "0000000000000000000000000000000000000000000000000000000000000001";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

std::string
req(std::string const& extra = {})
{
    return std::format(R"JSON({{"nft_id": "{}"{}}})JSON", kHex1, extra);
}

}  // namespace

TEST(NftOffersSpec, nft_id_required)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'nft_id'.");
}

TEST(NftOffersSpec, minimal_request_parses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
    EXPECT_FALSE(result->marker.has_value());
}

// --- nft_id -----------------------------------------------------------------

TEST(NftOffersSpec, malformed_nft_id_is_invalid_field)
{
    for (auto const* bad : {R"JSON("NOTHEX")JSON", "5", "null", "true", "[]", "{}"})
    {
        auto const result = parse(std::format(R"JSON({{"nft_id": {}}})JSON", bad));
        ASSERT_FALSE(result.has_value()) << "nft_id=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "nft_id=" << bad;
        EXPECT_EQ(result.error().message, "Invalid field 'nft_id'.") << "nft_id=" << bad;
    }
}

// --- limit ------------------------------------------------------------------

TEST(NftOffersSpec, limit_is_not_clamped_by_the_spec)
{
    auto const low = parse(req(R"JSON(, "limit": 1)JSON"));
    ASSERT_TRUE(low.has_value()) << "error: " << low.error().error
                                 << " msg: " << low.error().message;
    EXPECT_EQ(low->limit, 1u);

    auto const high = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(high.has_value());
    EXPECT_EQ(high->limit, 100000u);
}

TEST(NftOffersSpec, null_limit_is_the_default)
{
    auto const result = parse(req(R"JSON(, "limit": null)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
}

TEST(NftOffersSpec, limit_zero_is_rejected)
{
    auto const result = parse(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'limit'.");
}

TEST(NftOffersSpec, non_integer_limit_is_rejected)
{
    for (auto const* bad : {R"JSON("0")JSON", "-1", "1.5", "true"})
    {
        auto const result = parse(req(std::format(R"JSON(, "limit": {})JSON", bad)));
        ASSERT_FALSE(result.has_value()) << "limit=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "limit=" << bad;
        EXPECT_EQ(result.error().message, "Invalid field 'limit', not unsigned integer.")
            << "limit=" << bad;
    }
}

// --- marker -----------------------------------------------------------------

TEST(NftOffersSpec, hex_marker_parses)
{
    auto const result = parse(req(std::format(R"JSON(, "marker": "{}")JSON", kHex2)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHex2));
    EXPECT_EQ(*result->marker, expected);
}

TEST(NftOffersSpec, non_string_marker_is_not_string)
{
    for (auto const* bad : {"5", "null", "true", "[]"})
    {
        auto const result = parse(req(std::format(R"JSON(, "marker": {})JSON", bad)));
        ASSERT_FALSE(result.has_value()) << "marker=" << bad;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << "marker=" << bad;
        EXPECT_EQ(result.error().message, "Invalid field 'marker', not string.")
            << "marker=" << bad;
    }
}

TEST(NftOffersSpec, non_hex_marker_is_bare_invalid_params)
{
    auto const result = parse(req(R"JSON(, "marker": "NOTHEX")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty()) << "unexpected: " << result.error().message;
}
