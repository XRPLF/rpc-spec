#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

using namespace rpc::spec;

namespace {

inline constexpr uint32_t kLimitMin = 10;
inline constexpr uint32_t kLimitMax = 400;
inline constexpr uint32_t kLimitDefault = 200;

struct Input
{
    uint32_t limit;  // default supplied by the spec (defaultTo)
    std::optional<uint32_t> opt;
};

constexpr auto kSpec = spec<Input>(
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        defaultTo(kLimitDefault),
        asUint32),
    field("opt", &Input::opt, type<uint32_t>, asUint32));

Input
parse(char const* json)
{
    auto value = boost::json::parse(json);
    auto const result = kSpec.parse(value);
    EXPECT_TRUE(result.has_value());
    return *result;
}

template <typename V>
concept CanParse = requires(V& value) { kSpec.parse(value); };

}  // namespace

TEST(RpcSpecDSLDefault, parse_rejects_a_const_document)
{
    static_assert(CanParse<boost::json::value>);
    static_assert(not CanParse<boost::json::value const>);
}

TEST(RpcSpecDSLDefault, absent_field_receives_spec_default)
{
    auto const in = parse(R"JSON({})JSON");
    EXPECT_EQ(in.limit, kLimitDefault);
}

TEST(RpcSpecDSLDefault, present_value_overrides_default)
{
    auto const in = parse(R"JSON({ "limit": 50 })JSON");
    EXPECT_EQ(in.limit, 50u);
}

TEST(RpcSpecDSLDefault, present_value_is_still_clamped_not_defaulted)
{
    // A present-but-out-of-range value is clamped; the default never enters.
    EXPECT_EQ(parse(R"JSON({ "limit": 9999 })JSON").limit, kLimitMax);
    EXPECT_EQ(parse(R"JSON({ "limit": 1 })JSON").limit, kLimitMin);
}

TEST(RpcSpecDSLDefault, default_does_not_suppress_requirement_errors)
{
    // `min(1)` still runs even when the field is present and invalid; the default
    // is applied only on the absent branch, after items pass.
    auto value = boost::json::parse(R"JSON({ "limit": 0 })JSON");
    auto const result = kSpec.parse(value);
    EXPECT_FALSE(result.has_value());
}

TEST(RpcSpecDSLDefault, optional_member_without_default_stays_nullopt)
{
    auto const in = parse(R"JSON({ "limit": 50 })JSON");
    EXPECT_FALSE(in.opt.has_value());
}

TEST(RpcSpecDSLDefault, dump_renders_default_value)
{
    std::ostringstream oss;
    SpecDumpWriter writer{oss};
    kSpec.dump(writer);
    EXPECT_TRUE(oss.str().contains("default"));
    EXPECT_TRUE(oss.str().contains("value: 200"));
}
