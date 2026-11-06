#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/JsonVisitor.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/Rule.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/Tunable.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using admission::spec::AdmissionDecision;
using admission::spec::EventKind;
using admission::spec::Rule;
using admission::spec::VisitEvent;

namespace {

std::span<uint8_t const>
asBytes(std::string_view json)
{
    return {reinterpret_cast<uint8_t const*>(json.data()), json.size()};
}

struct RecordedEvent
{
    EventKind kind{};
    std::string key;
    size_t size{};
    uint32_t depth{};

    bool
    operator==(RecordedEvent const&) const = default;
};

// A request with a spec: at most two `items`, each at least two characters, and a `limit` of at
// most five.
struct ListRequest
{
};

consteval auto
admissionSpec(std::type_identity<ListRequest>)
{
    return admission::spec::makeSpec<ListRequest>(
               admission::spec::tunable<"max_payload_bytes">(
                   uint64_t{1024}, "admission.list.max_payload_bytes"),
               admission::spec::tunable<"size_ramp">(
                   admission::spec::ramp({{.upToBytes = 1024, .cost = 1.0}}),
                   "admission.list.size_ramp"))
        .withAttributes(
            {.typeId = 1,
             .repeated = admission::spec::repeatedCaps(
                 {{.key = "items", .maxEntries = 2, .minEntryBytes = 2}})})
        .withCheck([](VisitEvent const& event, auto const&) -> AdmissionDecision {
            if (auto const* limit = event.as<int64_t>();
                event.depth == 1 and event.key == "limit" and limit != nullptr and *limit > 5)
            {
                return AdmissionDecision::drop(
                    "limit too large", admission::spec::PenaltyClass::Invalid);
            }
            return AdmissionDecision::admit();
        });
}

}  // namespace

TEST(JsonVisitor, prescan_json_applies_a_specs_caps_and_check)
{
    using admission::spec::prescanJson;
    EXPECT_TRUE(prescanJson<ListRequest>(asBytes(R"({"items":["ab","cd"],"limit":5})")).admitted());
    EXPECT_EQ(
        prescanJson<ListRequest>(asBytes(R"({"items":["ab","cd","ef"]})")).rule, Rule::RepeatedCap);
    EXPECT_EQ(prescanJson<ListRequest>(asBytes(R"({"items":["a"]})")).rule, Rule::EntrySize);
    EXPECT_EQ(prescanJson<ListRequest>(asBytes(R"({"limit":6})")).rule, Rule::Check);
    EXPECT_EQ(prescanJson<ListRequest>(asBytes(R"({"limit":)")).rule, Rule::Malformed);
}

TEST(JsonVisitor, json_visitor_walks_with_the_check_it_is_given_and_its_options)
{
    auto const body = asBytes(R"({"a":[[1]]})");
    auto seen = 0;
    auto count = [&](VisitEvent const&) {
        ++seen;
        return AdmissionDecision::admit();
    };
    EXPECT_TRUE(admission::spec::jsonVisitor(body)(count).admitted());
    EXPECT_GT(seen, 0);

    // Its options reach the walk: nesting deeper than allowed is invalid JSON.
    auto const shallow = admission::spec::jsonVisitor(
        body, admission::spec::VisitJsonOptions{.costForInvalidPayload = 4.0, .maxDepth = 2});
    auto const refused = shallow(count);
    EXPECT_EQ(refused.rule, Rule::Malformed);
    EXPECT_EQ(refused.tokenCost, 4.0);
}

TEST(JsonVisitor, emits_keyed_events_with_depth_and_size)
{
    using admission::spec::visitJson;
    using enum EventKind;

    auto events = std::vector<RecordedEvent>{};
    auto record = [&](VisitEvent const& event) {
        events.push_back(
            {.kind = event.kind,
             .key = std::string{event.key},
             .size = event.size,
             .depth = event.depth});
        return AdmissionDecision::admit();
    };

    auto const decision =
        visitJson(asBytes(R"JSON({"id": "abc", "items": [1, -2], "m": {"p": true}})JSON"), record);
    EXPECT_TRUE(decision.admitted());
    EXPECT_EQ(
        events,
        (std::vector<RecordedEvent>{
            {ObjectBegin, "", 0, 0},
            {Key, "", 2, 1},
            {String, "id", 3, 1},
            {Key, "", 5, 1},
            {ArrayBegin, "items", 0, 1},
            {Int64, "", 0, 2},
            {Int64, "", 0, 2},
            {ArrayEnd, "items", 2, 1},
            {Key, "", 1, 1},
            {ObjectBegin, "m", 0, 1},
            {Key, "", 1, 2},
            {Bool, "p", 0, 2},
            {ObjectEnd, "m", 1, 1},
            {ObjectEnd, "", 3, 0},
        }));
}

TEST(JsonVisitor, carries_typed_values)
{
    using admission::spec::visitJson;

    auto strings = std::vector<std::string>{};
    auto ints = std::vector<int64_t>{};
    auto uints = std::vector<uint64_t>{};
    auto doubles = std::vector<double>{};
    auto nulls = 0;
    auto record = [&](VisitEvent const& event) {
        if (event.kind == EventKind::String)
        {
            strings.emplace_back(*event.as<std::string_view>());
        }
        else if (auto const* i = event.as<int64_t>(); i != nullptr)
        {
            ints.push_back(*i);
        }
        else if (auto const* u = event.as<uint64_t>(); u != nullptr)
        {
            uints.push_back(*u);
        }
        else if (auto const* d = event.as<double>(); d != nullptr)
        {
            doubles.push_back(*d);
        }
        else if (event.kind == EventKind::Null)
        {
            ++nulls;
        }
        return AdmissionDecision::admit();
    };

    auto const decision =
        visitJson(asBytes(R"JSON(["s", -1, 18446744073709551615, 1.5, null])JSON"), record);
    EXPECT_TRUE(decision.admitted());
    EXPECT_EQ(strings, (std::vector<std::string>{"s"}));
    EXPECT_EQ(ints, (std::vector<int64_t>{-1}));
    EXPECT_EQ(uints, (std::vector<uint64_t>{18446744073709551615u}));
    EXPECT_EQ(doubles, (std::vector<double>{1.5}));
    EXPECT_EQ(nulls, 1);
}

// A drop stops the parse at that event and is returned as-is — not masked as invalid JSON.
TEST(JsonVisitor, stops_on_first_drop)
{
    using admission::spec::visitJson;

    auto seen = 0;
    auto dropOnItems = [&](VisitEvent const& event) {
        ++seen;
        return event.key == "items" ? AdmissionDecision::drop("no items", 3.0)
                                    : AdmissionDecision::admit();
    };

    auto const decision =
        visitJson(asBytes(R"JSON({"items": [1, 2, 3], "after": 1})JSON"), dropOnItems);
    EXPECT_TRUE(decision.dropped());
    EXPECT_EQ(decision.reason, "no items");
    EXPECT_EQ(decision.tokenCost, 3.0);
    // A drop that names no rule is the check's own.
    EXPECT_EQ(decision.rule, Rule::Check);
    EXPECT_EQ(seen, 3);  // ObjectBegin, Key, ArrayBegin (drop) — nothing after
}

TEST(JsonVisitor, drops_malformed_payloads)
{
    using admission::spec::visitJson;
    using admission::spec::VisitJsonOptions;

    auto admitAll = [](VisitEvent const&) { return AdmissionDecision::admit(); };
    auto const options = VisitJsonOptions{.costForInvalidPayload = 7.0, .maxDepth = 2};

    for (auto const json : {
             R"JSON({"id": "abc",)JSON",      // truncated
             R"JSON({"a": 1} trailing)JSON",  // data after the document
             R"JSON({"a": [[1]]})JSON",       // deeper than maxDepth
             R"JSON({'a': 1})JSON",           // not JSON
         })
    {
        auto const decision = visitJson(asBytes(json), admitAll, options);
        EXPECT_TRUE(decision.dropped()) << json;
        EXPECT_EQ(decision.reason, "Invalid JSON payload") << json;
        EXPECT_EQ(decision.tokenCost, 7.0) << json;
        EXPECT_EQ(decision.rule, Rule::Malformed) << json;
    }

    // Trailing whitespace is not trailing data.
    EXPECT_TRUE(visitJson(asBytes("{\"a\": 1} \n\t"), admitAll, options).admitted());
}
