#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>

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
    auto const r = kSpec.parse(value);
    EXPECT_TRUE(r.has_value());
    return *r;
}

}  // namespace

TEST(RpcSpecDSL_Default, AbsentFieldReceivesSpecDefault)
{
    auto const in = parse(R"JSON({})JSON");
    EXPECT_EQ(in.limit, kLimitDefault);
}

TEST(RpcSpecDSL_Default, PresentValueOverridesDefault)
{
    auto const in = parse(R"JSON({ "limit": 50 })JSON");
    EXPECT_EQ(in.limit, 50u);
}

TEST(RpcSpecDSL_Default, PresentValueIsStillClampedNotDefaulted)
{
    // A present-but-out-of-range value is clamped; the default never enters.
    EXPECT_EQ(parse(R"JSON({ "limit": 9999 })JSON").limit, kLimitMax);
    EXPECT_EQ(parse(R"JSON({ "limit": 1 })JSON").limit, kLimitMin);
}

TEST(RpcSpecDSL_Default, DefaultDoesNotSuppressRequirementErrors)
{
    // `min(1)` still runs even when the field is present and invalid; the default
    // is applied only on the absent branch, after items pass.
    auto value = boost::json::parse(R"JSON({ "limit": 0 })JSON");
    auto const r = kSpec.parse(value);
    EXPECT_FALSE(r.has_value());
}

TEST(RpcSpecDSL_Default, OptionalMemberWithoutDefaultStaysNullopt)
{
    auto const in = parse(R"JSON({ "limit": 50 })JSON");
    EXPECT_FALSE(in.opt.has_value());
}

TEST(RpcSpecDSL_Default, DumpRendersDefaultValue)
{
    std::ostringstream oss;
    SpecDumpWriter w{oss};
    kSpec.dump(w);
    EXPECT_NE(oss.str().find("default"), std::string::npos);
    EXPECT_NE(oss.str().find("value: 200"), std::string::npos);
}
