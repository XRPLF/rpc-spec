/**
 * @file
 *  GTest coverage for the lenient `jsonBool` converter and the JsonBool type.
 *  Compiled under RPCSPEC_IS_XRPLD.
 */
#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Converters.hpp>
#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Typed.hpp>

using namespace rpc::spec;

namespace {
struct FlagInput
{
    JsonBool flag{false};
};

constexpr auto kFlagSpec = spec<FlagInput>(field("flag", &FlagInput::flag, jsonBool));

JsonBool
parseFlag(char const* json)
{
    auto value = boost::json::parse(json);
    auto const r = kFlagSpec.parse(value);
    EXPECT_TRUE(r.has_value());
    return r->flag;
}
}  // namespace

TEST(JsonBoolConverter, NullIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": null })JSON")));
}

TEST(JsonBoolConverter, BoolTrueIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": true })JSON")));
}

TEST(JsonBoolConverter, BoolFalseIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": false })JSON")));
}

TEST(JsonBoolConverter, NonZeroIntIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": 1 })JSON")));
}

TEST(JsonBoolConverter, ZeroIntIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": 0 })JSON")));
}

TEST(JsonBoolConverter, NonZeroDoubleIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": 0.1 })JSON")));
}

TEST(JsonBoolConverter, ZeroDoubleIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": 0.0 })JSON")));
}

TEST(JsonBoolConverter, NonEmptyStringIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": "true" })JSON")));
}

// Deliberate: any non-empty string is truthy, so "false" is true. xrpld does not special-case
// the literal, and API v2 rejects non-bools outright via `jsonBoolStrict`.
TEST(JsonBoolConverter, StringFalseIsAlsoTrue)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": "false" })JSON")));
}

TEST(JsonBoolConverter, NonEmptyObjectIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": {"a": 1} })JSON")));
}

TEST(JsonBoolConverter, EmptyObjectIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": {} })JSON")));
}

TEST(JsonBoolConverter, NonEmptyArrayIsTrue)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": [1] })JSON")));
}

TEST(JsonBoolConverter, EmptyArrayIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": [] })JSON")));
}
