/**
 * @file
 *  GTest coverage for the `account_offers` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 *
 *  `AccountMarkerStrConverter` re-implements the shared `accountMarker`
 *  validator's parse inside a converter (it returns the string rather than
 *  merely accepting it), so its arms are pinned independently here.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/account_offers/Spec.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <format>
#include <initializer_list>
#include <string>

using namespace rpc::spec;
using namespace rpc::spec::handlers::account_offers;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kHex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kPublicKeyHex = "0330E7FC9D56BB25D6893BA3F317AE5BCF33B3291BD63DB32654A313222F7FD020";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpecV1.parse(value);
}

std::string
req(std::string const& extra = {})
{
    return std::format(R"JSON({{"account": "{}"{}}})JSON", kAcct1, extra);
}

std::string
withMarker(std::string const& marker)
{
    return req(std::format(R"JSON(, "marker": "{}")JSON", marker));
}

}  // namespace

TEST(AccountOffersSpec, account_required)
{
    auto const result = parse(R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'account'.");
}

TEST(AccountOffersSpec, minimal_request_parses)
{
    auto const result = parse(req());
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
    EXPECT_FALSE(result->marker.has_value());
}

TEST(AccountOffersSpec, malformed_account_is_left_to_the_handler)
{
    auto const result = parse(R"JSON({"account": "notanaccount"})JSON");
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_FALSE(result->account.has_value());
    EXPECT_EQ(result->account.error(), AccountError::Malformed);
}

TEST(AccountOffersSpec, non_string_account_is_invalid_params)
{
    auto const result = parse(R"JSON({"account": 5})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'account'.");
}

TEST(AccountOffersSpec, public_key_is_not_an_account)
{
    auto const result = parse(std::format(R"JSON({{"account": "{}"}})JSON", kPublicKeyHex));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_FALSE(result->account.has_value());
}

// --- limit ------------------------------------------------------------------

TEST(AccountOffersSpec, limit_is_not_clamped_by_the_spec)
{
    auto const low = parse(req(R"JSON(, "limit": 5)JSON"));
    ASSERT_TRUE(low.has_value()) << "error: " << low.error().error
                                 << " msg: " << low.error().message;
    EXPECT_EQ(low->limit, 5u);

    auto const high = parse(req(R"JSON(, "limit": 100000)JSON"));
    ASSERT_TRUE(high.has_value());
    EXPECT_EQ(high->limit, 100000u);
}

TEST(AccountOffersSpec, null_limit_is_the_default)
{
    auto const result = parse(req(R"JSON(, "limit": null)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    EXPECT_EQ(result->limit, kLimitDefault);
}

TEST(AccountOffersSpec, limit_zero_is_rejected)
{
    auto const result = parse(req(R"JSON(, "limit": 0)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'limit'.");
}

TEST(AccountOffersSpec, non_integer_limit_is_rejected)
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

// --- AccountMarkerStrConverter ---------------------------------------------

TEST(AccountOffersSpec, well_formed_marker_round_trips_as_string)
{
    auto const marker = std::format("{},7", kHex1);
    auto const result = parse(withMarker(marker));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(*result->marker, marker);
}

TEST(AccountOffersSpec, marker_hint_zero_is_accepted)
{
    auto const result = parse(withMarker(std::format("{},0", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AccountOffersSpec, non_string_marker_names_the_field)
{
    auto const result = parse(req(R"JSON(, "marker": 5)JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker', not string.");
}

TEST(AccountOffersSpec, marker_without_comma_is_malformed_cursor)
{
    auto const result = parse(withMarker(kHex1));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, marker_with_bad_hex_is_malformed_cursor)
{
    auto const result = parse(withMarker("NOTHEX,7"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, marker_with_empty_hint_is_malformed_cursor)
{
    auto const result = parse(withMarker(std::format("{},", kHex1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, marker_with_trailing_garbage_after_hint_is_malformed_cursor)
{
    auto const result = parse(withMarker(std::format("{},7x", kHex1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, marker_with_negative_hint_is_malformed_cursor)
{
    // from_chars into uint64_t rejects a leading '-'.
    auto const result = parse(withMarker(std::format("{},-1", kHex1)));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(AccountOffersSpec, marker_hint_may_have_a_leading_plus)
{
    auto const result = parse(withMarker(std::format("{},+7", kHex1)));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AccountOffersSpec, marker_text_after_a_second_comma_is_ignored)
{
    auto const marker = std::format("{},7,anything", kHex1);
    auto const result = parse(withMarker(marker));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(*result->marker, marker);
}

TEST(AccountOffersSpec, deprecated_fields_do_not_fail_the_request)
{
    auto const result = parse(req(R"JSON(, "ledger": 5, "strict": true)JSON"));
    ASSERT_TRUE(result.has_value())
        << "error: " << result.error().error << " msg: " << result.error().message;
}

TEST(AccountOffersSpec, deprecated_fields_do_not_warn)
{
    auto const request = boost::json::parse(req(R"JSON(, "ledger": 5, "strict": true)JSON"));
    EXPECT_TRUE(kInputSpecV1.check(request).empty());
}
