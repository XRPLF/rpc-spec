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

#include <Backend.hpp>  // IWYU pragma: keep

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
    auto const result = kFlagSpec.parse(value);
    EXPECT_TRUE(result.has_value());
    return result->flag;
}
}  // namespace

TEST(JsonBoolConverter, null_is_false)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": null })JSON")));
}

TEST(JsonBoolConverter, bool_true_is_true)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": true })JSON")));
}

TEST(JsonBoolConverter, bool_false_is_false)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": false })JSON")));
}

TEST(JsonBoolConverter, non_zero_int_is_true)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": 1 })JSON")));
}

TEST(JsonBoolConverter, zero_int_is_false)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": 0 })JSON")));
}

TEST(JsonBoolConverter, non_zero_double_is_true)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": 0.1 })JSON")));
}

TEST(JsonBoolConverter, zero_double_is_false)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": 0.0 })JSON")));
}

TEST(JsonBoolConverter, non_empty_string_is_true)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": "true" })JSON")));
}

// Deliberate: any non-empty string is truthy, so "false" is true. xrpld does not special-case
// the literal, and API v2 rejects non-bools outright via `jsonBoolStrict`.
TEST(JsonBoolConverter, string_false_is_also_true)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": "false" })JSON")));
}

TEST(JsonBoolConverter, non_empty_object_is_true)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": {"a": 1} })JSON")));
}

TEST(JsonBoolConverter, empty_object_is_false)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": {} })JSON")));
}

TEST(JsonBoolConverter, non_empty_array_is_true)
{
    EXPECT_TRUE(static_cast<bool>(parseFlag(R"JSON({ "flag": [1] })JSON")));
}

TEST(JsonBoolConverter, empty_array_is_false)
{
    EXPECT_FALSE(static_cast<bool>(parseFlag(R"JSON({ "flag": [] })JSON")));
}
