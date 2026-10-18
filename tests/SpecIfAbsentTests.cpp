/**
 * @file
 * @brief Tests for the `ifAbsent(guardKey)` guard: a bound field processed only when
 * another root key is absent, on both servers or, via `ifServer*`, on one. Also covers the
 * `never` guard and two keys binding one member.
 */

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <cstdint>
#include <expected>
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
    std::optional<uint32_t> value;
};

// The guard key is deliberately not bound: ifAbsent only inspects its presence.
constexpr auto kSpec = spec<Input>(field("value", &Input::value, ifAbsent("guard"), asUint32));

constexpr auto kRequiredSpec = spec<Input>(field(
    "value",
    &Input::value,
    ifAbsent("guard"),
    withCustomError(required, rpc::XrpldError::RpcInvalidParams, "Missing field 'guard'."),
    asUint32));

constexpr auto kDeprecatedSpec =
    spec<Input>(field("value", &Input::value, ifAbsent("guard"), deprecated, asUint32));

struct PrimaryFallbackInput
{
    std::optional<std::string> primary;
    std::optional<std::string> fallback;
};

// The account/ident shape: `fallback` is read only when `primary` is not sent.
constexpr auto kFallbackSpec = spec<PrimaryFallbackInput>(
    field("primary", &PrimaryFallbackInput::primary, asString),
    field(
        "fallback",
        &PrimaryFallbackInput::fallback,
        ifAbsent("primary"),
        withCustomError(required, rpc::XrpldError::RpcInvalidParams, "Missing field 'primary'."),
        asString));

std::expected<Input, rpc::Status>
parse(auto const& spec, char const* json)
{
    auto value = boost::json::parse(json);
    return spec.parse(value);
}

}  // namespace

// --- binding ------------------------------------------------------------------

static_assert(SomeGuard<decltype(ifAbsent("guard"))>);
static_assert(SomeFieldItem<decltype(ifAbsent("guard"))>);
static_assert(SomeGuard<decltype(ifServerXrpld(ifAbsent("guard")))>);
static_assert(SomeGuard<decltype(ifServerClio(ifAbsent("guard")))>);

TEST(RpcSpecDSLIfAbsent, guarded_field_keeps_its_own_key)
{
    constexpr auto guarded = field("value", &Input::value, ifAbsent("guard"), asUint32);
    EXPECT_EQ(guarded.key, "value");
    EXPECT_EQ(std::get<0>(guarded.items).guardKey, "guard");
}

// --- guard absent: behaves like an unguarded field -------------------------------

TEST(RpcSpecDSLIfAbsent, field_is_parsed_when_guard_is_absent)
{
    auto const result = parse(kSpec, R"JSON({ "value": 42 })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->value.has_value());
    EXPECT_EQ(*result->value, 42u);
}

TEST(RpcSpecDSLIfAbsent, absent_optional_field_stays_nullopt_when_guard_is_absent)
{
    auto const result = parse(kSpec, R"JSON({})JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, field_error_is_reported_when_guard_is_absent)
{
    auto const result = parse(kSpec, R"JSON({ "value": "not a number" })JSON");
    ASSERT_FALSE(result.has_value());
}

TEST(RpcSpecDSLIfAbsent, requirement_is_enforced_when_guard_is_absent)
{
    auto const result = parse(kRequiredSpec, R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'guard'.");
}

TEST(RpcSpecDSLIfAbsent, non_object_root_runs_the_field)
{
    // A non-object root has no keys, so the guard is absent and `required` fires.
    auto const result = parse(kRequiredSpec, R"JSON([])JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Missing field 'guard'.");
}

// --- guard present: the field is ignored entirely --------------------------------

TEST(RpcSpecDSLIfAbsent, field_is_not_read_when_guard_is_present)
{
    auto const result = parse(kSpec, R"JSON({ "guard": 1, "value": 42 })JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, invalid_field_is_not_an_error_when_guard_is_present)
{
    for (auto const* json :
         {R"JSON({ "guard": 1, "value": "not a number" })JSON",
          R"JSON({ "guard": 1, "value": -5 })JSON",
          R"JSON({ "guard": 1, "value": {} })JSON"})
    {
        auto const result = parse(kSpec, json);
        ASSERT_TRUE(result.has_value()) << json;
        EXPECT_FALSE(result->value.has_value()) << json;
    }
}

TEST(RpcSpecDSLIfAbsent, requirement_is_skipped_when_guard_is_present)
{
    auto const result = parse(kRequiredSpec, R"JSON({ "guard": 1 })JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, any_guard_value_counts_as_present)
{
    // Presence is the only thing tested: falsy, empty and null guards all disable the field.
    for (auto const* json :
         {R"JSON({ "guard": null })JSON",
          R"JSON({ "guard": false })JSON",
          R"JSON({ "guard": 0 })JSON",
          R"JSON({ "guard": "" })JSON",
          R"JSON({ "guard": [] })JSON",
          R"JSON({ "guard": {} })JSON"})
    {
        auto const result = parse(kRequiredSpec, json);
        ASSERT_TRUE(result.has_value()) << json;
        EXPECT_FALSE(result->value.has_value()) << json;
    }
}

// --- defaults and modifiers ---------------------------------------------------

namespace {

struct LimitInput
{
    uint32_t limit;
};

constexpr auto kLimitSpec = spec<LimitInput>(field(
    "limit",
    &LimitInput::limit,
    ifAbsent("guard"),
    clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
    defaultTo(kLimitDefault),
    asUint32));

}  // namespace

TEST(RpcSpecDSLIfAbsent, default_applies_when_guard_and_field_are_absent)
{
    auto value = boost::json::parse(R"JSON({})JSON");
    auto const result = kLimitSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, kLimitDefault);
}

TEST(RpcSpecDSLIfAbsent, default_is_not_applied_when_guard_is_present)
{
    // The whole field is skipped, so the member keeps its value-initialised state.
    auto value = boost::json::parse(R"JSON({ "guard": true })JSON");
    auto const result = kLimitSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, 0u);
}

TEST(RpcSpecDSLIfAbsent, modifiers_run_when_guard_is_absent)
{
    auto value = boost::json::parse(R"JSON({ "limit": 9999 })JSON");
    auto const result = kLimitSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->limit, kLimitMax);
    EXPECT_EQ(value.as_object().at("limit").to_number<uint32_t>(), kLimitMax);
}

TEST(RpcSpecDSLIfAbsent, modifiers_do_not_touch_the_request_when_guard_is_present)
{
    auto value = boost::json::parse(R"JSON({ "guard": true, "limit": 9999 })JSON");
    auto const result = kLimitSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(value.as_object().at("limit").to_number<uint32_t>(), 9999u);
}

// --- warnings -----------------------------------------------------------------

TEST(RpcSpecDSLIfAbsent, warnings_are_reported_when_guard_is_absent)
{
    auto const request = boost::json::parse(R"JSON({ "value": 1 })JSON");
    auto const warnings = kDeprecatedSpec.check(request);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].field, "value");
    EXPECT_EQ(warnings[0].code, rpc::WarningCode::WarnRpcDeprecated);
}

TEST(RpcSpecDSLIfAbsent, warnings_are_suppressed_when_guard_is_present)
{
    auto const request = boost::json::parse(R"JSON({ "guard": 1, "value": 1 })JSON");
    EXPECT_TRUE(kDeprecatedSpec.check(request).empty());
}

TEST(RpcSpecDSLIfAbsent, no_warning_when_field_is_absent)
{
    auto const request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kDeprecatedSpec.check(request).empty());
}

// --- primary / fallback pattern -----------------------------------------------

TEST(RpcSpecDSLIfAbsent, fallback_is_read_when_primary_is_absent)
{
    auto value = boost::json::parse(R"JSON({ "fallback": "b" })JSON");
    auto const result = kFallbackSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->primary.has_value());
    EXPECT_EQ(result->fallback, "b");
}

TEST(RpcSpecDSLIfAbsent, fallback_is_ignored_when_primary_is_present)
{
    auto value = boost::json::parse(R"JSON({ "primary": "a", "fallback": 5 })JSON");
    auto const result = kFallbackSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->primary, "a");
    EXPECT_FALSE(result->fallback.has_value());
}

TEST(RpcSpecDSLIfAbsent, guard_field_is_still_validated_by_its_own_binding)
{
    auto value = boost::json::parse(R"JSON({ "primary": 5, "fallback": "b" })JSON");
    EXPECT_FALSE(kFallbackSpec.parse(value).has_value());
}

TEST(RpcSpecDSLIfAbsent, neither_primary_nor_fallback_reports_missing_primary)
{
    auto value = boost::json::parse(R"JSON({})JSON");
    auto const result = kFallbackSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'primary'.");
}

// --- two keys, one member -------------------------------------------------------

namespace {

struct SharedInput
{
    std::optional<std::string> value;
};

// The account/ident shape: `fallback` feeds the member only when `primary` is not sent.
constexpr auto kSharedSpec = spec<SharedInput>(
    field("fallback", &SharedInput::value, ifAbsent("primary"), asString),
    field("primary", &SharedInput::value, asString));

static_assert(
    distinctBoundMemberCount(
        field("fallback", &SharedInput::value, asString),
        field("primary", &SharedInput::value, asString)) == 1);

}  // namespace

TEST(RpcSpecDSLIfAbsent, shared_member_is_read_from_the_primary_key)
{
    auto value = boost::json::parse(R"JSON({ "primary": "a", "fallback": "b" })JSON");
    auto const result = kSharedSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, "a");
}

TEST(RpcSpecDSLIfAbsent, shared_member_is_read_from_the_fallback_key)
{
    auto value = boost::json::parse(R"JSON({ "fallback": "b" })JSON");
    auto const result = kSharedSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, "b");
}

TEST(RpcSpecDSLIfAbsent, shared_member_stays_unset_when_neither_key_is_sent)
{
    auto value = boost::json::parse(R"JSON({})JSON");
    auto const result = kSharedSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->value.has_value());
}

// --- never --------------------------------------------------------------------

namespace {

constexpr auto kNeverSpec = spec<Input>(field("value", &Input::value, never, deprecated, asUint32));

}  // namespace

TEST(RpcSpecDSLIfAbsent, never_skips_the_field)
{
    auto const result = parse(kNeverSpec, R"JSON({ "value": "not a number" })JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, never_suppresses_warnings)
{
    auto const request = boost::json::parse(R"JSON({ "value": 7 })JSON");
    EXPECT_TRUE(kNeverSpec.check(request).empty());
}

// --- ordering -----------------------------------------------------------------

namespace {

struct OrderedInput
{
    std::optional<uint32_t> first;
    std::optional<uint32_t> value;
    std::optional<uint32_t> last;
};

constexpr auto kOrderedSpec = spec<OrderedInput>(
    field("first", &OrderedInput::first, asUint32),
    field(
        "value",
        &OrderedInput::value,
        ifAbsent("guard"),
        withCustomError(required, rpc::XrpldError::RpcInvalidParams, "value"),
        asUint32),
    field(
        "last",
        &OrderedInput::last,
        withCustomError(type<uint32_t>, rpc::XrpldError::RpcInvalidParams, "last"),
        asUint32));

}  // namespace

TEST(RpcSpecDSLIfAbsent, earlier_field_error_wins_over_guarded_field)
{
    auto value = boost::json::parse(R"JSON({ "first": "x" })JSON");
    auto const result = kOrderedSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message, "value");
}

TEST(RpcSpecDSLIfAbsent, guarded_field_error_wins_over_later_field)
{
    auto value = boost::json::parse(R"JSON({ "last": "x" })JSON");
    auto const result = kOrderedSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "value");
}

TEST(RpcSpecDSLIfAbsent, skipped_field_lets_later_fields_run)
{
    auto value = boost::json::parse(R"JSON({ "guard": 1, "last": "x" })JSON");
    auto const result = kOrderedSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "last");
}

// --- composition --------------------------------------------------------------

namespace {

constexpr auto kAliasedSpec =
    spec<Input>(alias("alt", field("value", &Input::value, ifAbsent("guard"), asUint32)));

constexpr auto kTwoGuardsSpec =
    spec<Input>(field("value", &Input::value, ifAbsent("a"), ifAbsent("b"), asUint32));

constexpr auto kGuardLastSpec =
    spec<Input>(field("value", &Input::value, deprecated, ifAbsent("guard"), asUint32));

}  // namespace

TEST(RpcSpecDSLIfAbsent, guards_an_aliased_field)
{
    auto const viaAlt = parse(kAliasedSpec, R"JSON({ "alt": 7 })JSON");
    ASSERT_TRUE(viaAlt.has_value());
    EXPECT_EQ(viaAlt->value, 7u);

    auto const guarded = parse(kAliasedSpec, R"JSON({ "guard": 1, "alt": 7 })JSON");
    ASSERT_TRUE(guarded.has_value());
    EXPECT_FALSE(guarded->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, guard_applies_wherever_it_is_listed)
{
    auto const request = boost::json::parse(R"JSON({ "guard": 1, "value": 1 })JSON");
    EXPECT_TRUE(kGuardLastSpec.check(request).empty());

    auto const guarded = parse(kGuardLastSpec, R"JSON({ "guard": 1, "value": "x" })JSON");
    ASSERT_TRUE(guarded.has_value());
    EXPECT_FALSE(guarded->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, several_guards_require_every_guard_to_be_absent)
{
    auto const none = parse(kTwoGuardsSpec, R"JSON({ "value": 7 })JSON");
    ASSERT_TRUE(none.has_value());
    EXPECT_EQ(none->value, 7u);

    for (auto const* json :
         {R"JSON({ "a": 1, "value": 7 })JSON",
          R"JSON({ "b": 1, "value": 7 })JSON",
          R"JSON({ "a": 1, "b": 1, "value": 7 })JSON"})
    {
        auto const result = parse(kTwoGuardsSpec, json);
        ASSERT_TRUE(result.has_value()) << json;
        EXPECT_FALSE(result->value.has_value()) << json;
    }
}

// --- ifServer* composition ------------------------------------------------------
// This binary is an xrpld build: the ifServerXrpld guard is active, the ifServerClio one is not.

namespace {

constexpr auto kXrpldGuardSpec = spec<Input>(field(
    "value",
    &Input::value,
    ifServerXrpld(
        ifAbsent("guard"),
        withCustomError(required, rpc::XrpldError::RpcInvalidParams, "Missing field 'guard'.")),
    ifServerClio(deprecated),
    asUint32));

constexpr auto kClioGuardSpec = spec<Input>(
    field("value", &Input::value, ifServerClio(ifAbsent("guard")), deprecated, asUint32));

}  // namespace

TEST(RpcSpecDSLIfAbsent, active_server_guard_skips_the_field)
{
    auto const result = parse(kXrpldGuardSpec, R"JSON({ "guard": 1, "value": "x" })JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, active_server_guard_keeps_its_sibling_requirement)
{
    auto const result = parse(kXrpldGuardSpec, R"JSON({})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Missing field 'guard'.");
}

TEST(RpcSpecDSLIfAbsent, inactive_server_guard_does_not_skip_the_field)
{
    auto const parsed = parse(kClioGuardSpec, R"JSON({ "guard": 1, "value": 7 })JSON");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->value, 7u);

    EXPECT_FALSE(parse(kClioGuardSpec, R"JSON({ "guard": 1, "value": "x" })JSON").has_value());

    auto const request = boost::json::parse(R"JSON({ "guard": 1, "value": 7 })JSON");
    EXPECT_EQ(kClioGuardSpec.check(request).size(), 1u);
}

// --- extend() overrides -------------------------------------------------------

namespace {

constexpr auto kPlainV1 = spec<Input>(field("value", &Input::value, asUint32));
constexpr auto kGuardedV2 =
    extend(kPlainV1, field("value", &Input::value, ifAbsent("guard"), asUint32));
constexpr auto kUnguardedV3 = extend(kGuardedV2, field("value", &Input::value, asUint32));

}  // namespace

TEST(RpcSpecDSLIfAbsent, extend_can_add_a_guard_to_an_existing_field)
{
    auto const v1 = parse(kPlainV1, R"JSON({ "guard": 1, "value": 7 })JSON");
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1->value, 7u);

    auto const v2 = parse(kGuardedV2, R"JSON({ "guard": 1, "value": 7 })JSON");
    ASSERT_TRUE(v2.has_value());
    EXPECT_FALSE(v2->value.has_value());
}

TEST(RpcSpecDSLIfAbsent, extend_can_remove_a_guard_from_an_existing_field)
{
    auto const v3 = parse(kUnguardedV3, R"JSON({ "guard": 1, "value": 7 })JSON");
    ASSERT_TRUE(v3.has_value());
    EXPECT_EQ(v3->value, 7u);
}

// --- dump ---------------------------------------------------------------------

TEST(RpcSpecDSLIfAbsent, dump_lists_the_guard_as_an_item)
{
    std::ostringstream oss;
    SpecDumpWriter writer{oss};
    kDeprecatedSpec.dump(writer);
    EXPECT_EQ(
        oss.str(),
        "- value:\n"
        "  - ifAbsent:\n"
        "    key: \"guard\"\n"
        "  - deprecated\n"
        "  - uint32\n");
}
