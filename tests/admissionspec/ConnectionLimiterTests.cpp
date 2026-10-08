#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/ConnectionLimiter.hpp>
#include <admissionspec/JsonVisitor.hpp>
#include <admissionspec/PassthroughVisitor.hpp>
#include <admissionspec/ProtobufVisitor.hpp>
#include <admissionspec/Types.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using admission::spec::AdmissionDecision;
using admission::spec::EventKind;
using admission::spec::VisitEvent;

namespace {

struct FooMessage
{
    int64_t foo{};
};

consteval auto
admissionSpec(std::type_identity<FooMessage>)
{
    using namespace admission::spec;
    return makeSpec<FooMessage>(
               tunable<"max_payload_bytes">(uint64_t{64 * 1024}, "admission.foo.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 1024, .cost = 0.5}, {.upToBytes = 64 * 1024, .cost = 10.0}}),
                   "admission.foo.size_ramp"),
               tunable<"max_foo_value">(int64_t{100}, "admission.foo.max_foo_value"))
        .withCheck([](VisitEvent const& event, auto const& cfg) -> AdmissionDecision {
            if (event.fieldNumber == 2)  // `foo`
            {
                if (event.kind != EventKind::Int64)
                {
                    return AdmissionDecision::drop("foo value is invalid", 25.0);
                }
                if (auto const* fooValue = event.as<int64_t>();
                    fooValue != nullptr and *fooValue > cfg.template get<"max_foo_value">())
                {
                    return AdmissionDecision::drop("foo value is invalid", 25.0);
                }
            }
            return AdmissionDecision::admit();
        });
}

[[nodiscard]] auto
fooVisitor(int64_t fooValue)
{
    return [fooValue](auto check) -> AdmissionDecision {
        return check(VisitEvent{.kind = EventKind::Int64, .fieldNumber = 2, .value = fooValue});
    };
}

struct ProtobufMessage
{
};

// Drives PROTOBUF only. Every field arrives keyed by field number; a length-delimited field
// arrives as Bytes, its `value` a byte span, which the check re-enters with the matching walker
// (visitProtobuf for the sub-message, visitPackedVarint for the packed list). `inMeta`
// distinguishes the top-level field 1 (id) from `meta`'s field 1 (priority).
//   message Bar { string id = 1; repeated int32 items = 2 [packed]; Meta meta = 3; }
//   message Meta { int32 priority = 1; }
struct ProtobufChecker
{
    bool inMeta{};
    size_t items{};

    template <typename Cfg>
    AdmissionDecision
    operator()(VisitEvent const& event, Cfg const& cfg)
    {
        auto self = [&](VisitEvent const& ev) { return (*this)(ev, cfg); };

        if (inMeta)
        {
            // Inside `meta`: priority is field 1.
            if (event.fieldNumber == 1)
            {
                if (auto const* value = event.as<int64_t>();
                    value != nullptr and *value > cfg.template get<"max_priority">())
                {
                    return AdmissionDecision::drop("priority too high", 8.0);
                }
            }
            return AdmissionDecision::admit();
        }

        switch (event.fieldNumber)
        {
            case 1:  // id: a length-delimited string
            {
                auto const* byte = event.as<std::span<uint8_t const>>();
                auto const id = byte != nullptr
                    ? std::string_view{reinterpret_cast<char const*>(byte->data()), byte->size()}
                    : std::string_view{};
                if (id.size() > cfg.template get<"max_id_len">())
                {
                    return AdmissionDecision::drop("id too long", 2.0);
                }
                return AdmissionDecision::admit();
            }
            case 2:  // items: packed repeated int32 (a byte span) — re-enter, counting each element
            {
                if (auto const* body = event.as<std::span<uint8_t const>>(); body != nullptr)
                {
                    return admission::spec::visitPackedVarint(*body, event.fieldNumber, self);
                }
                if (++items > cfg.template get<"max_items">())
                {
                    return AdmissionDecision::drop("too many items", 4.0);
                }
                return AdmissionDecision::admit();
            }
            case 3:  // meta: a sub-message — descend, bracketing with `inMeta`
            {
                if (auto const* body = event.as<std::span<uint8_t const>>(); body != nullptr)
                {
                    inMeta = true;
                    auto const decision = admission::spec::visitProtobuf(*body, self);
                    inMeta = false;
                    return decision;
                }
                return AdmissionDecision::admit();
            }
            default:
                return AdmissionDecision::admit();
        }
    }
};

consteval auto
admissionSpec(std::type_identity<ProtobufMessage>)
{
    using namespace admission::spec;
    return makeSpec<ProtobufMessage>(
               tunable<"max_payload_bytes">(
                   uint64_t{64 * 1024}, "admission.protobuf_message.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 1024, .cost = 0.5}, {.upToBytes = 64 * 1024, .cost = 4.0}}),
                   "admission.protobuf_message.size_ramp"),
               tunable<"max_id_len">(size_t{8}, "admission.protobuf_message.max_id_len"),
               tunable<"max_items">(size_t{3}, "admission.protobuf_message.max_items"),
               tunable<"max_priority">(int64_t{5}, "admission.protobuf_message.max_priority"))
        .withCheck(ProtobufChecker{});
}

struct JsonMessage
{
};

// Drives JSON only. Keyed by object key; containers are bracketed by Begin/End, and the check
// tracks which one it is inside.  { "id": ..., "items": [ ... ], "meta": { "priority": ... } }
struct JsonChecker
{
    bool inItems{};
    bool inMeta{};
    size_t items{};

    template <typename Cfg>
    AdmissionDecision
    operator()(VisitEvent const& event, Cfg const& cfg)
    {
        switch (event.kind)
        {
            using enum EventKind;
            case ArrayBegin:
                if (event.key == "items")
                {
                    inItems = true;
                    items = 0;
                }
                return AdmissionDecision::admit();
            case ArrayEnd:
                if (event.key == "items")
                {
                    inItems = false;
                }
                return AdmissionDecision::admit();
            case ObjectBegin:
                if (event.key == "meta")
                {
                    inMeta = true;
                }
                return AdmissionDecision::admit();
            case ObjectEnd:
                if (event.key == "meta")
                {
                    inMeta = false;
                }
                return AdmissionDecision::admit();
            case String:
            case Int64:
                if (event.key == "id")
                {
                    if (event.kind == String and event.size > cfg.template get<"max_id_len">())
                    {
                        return AdmissionDecision::drop("id too long", 2.0);
                    }
                    return AdmissionDecision::admit();
                }
                if (inItems)
                {
                    if (++items > cfg.template get<"max_items">())
                    {
                        return AdmissionDecision::drop("too many items", 4.0);
                    }
                    return AdmissionDecision::admit();
                }
                if (inMeta and event.key == "priority")
                {
                    if (auto const* value = event.as<int64_t>();
                        value != nullptr and *value > cfg.template get<"max_priority">())
                    {
                        return AdmissionDecision::drop("priority too high", 8.0);
                    }
                }
                return AdmissionDecision::admit();
            default:
                return AdmissionDecision::admit();
        }
    }
};

consteval auto
admissionSpec(std::type_identity<JsonMessage>)
{
    using namespace admission::spec;
    return makeSpec<JsonMessage>(
               tunable<"max_payload_bytes">(
                   uint64_t{64 * 1024}, "admission.json_message.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 1024, .cost = 0.5}, {.upToBytes = 64 * 1024, .cost = 4.0}}),
                   "admission.json_message.size_ramp"),
               tunable<"max_id_len">(size_t{8}, "admission.json_message.max_id_len"),
               tunable<"max_items">(size_t{3}, "admission.json_message.max_items"),
               tunable<"max_priority">(int64_t{5}, "admission.json_message.max_priority"))
        .withCheck(JsonChecker{});
}

}  // namespace

TEST(ConnectionLimiterTests, rate_limit)
{
    auto bucketSettings =
        admission::spec::BucketSettings{.capacity = 50, .refillRatePerSecond = 10};
    auto limiter = admission::spec::ConnectionLimiter<size_t>{bucketSettings, 5};

    auto const start = std::chrono::steady_clock::now();

    {
        // Verify that the built-in pre-deserialization size gate drops an oversize payload.
        auto tooLarge = std::array<uint8_t, (64 * 1024) + 1>{};
        auto decision = limiter.admitPre<FooMessage>(0uz, tooLarge, start);
        EXPECT_FALSE(decision.admitted());
        EXPECT_TRUE(decision.dropped());
        EXPECT_EQ(decision.tokenCost, 10.0);
        EXPECT_EQ(decision.reason, "payload exceeds max bytes for this type");
        EXPECT_EQ(limiter.size(), 1uz);

        limiter.onDisconnect(0uz);
        EXPECT_EQ(limiter.size(), 0uz);
    }

    {
        // Verify that a droppable streaming check drops the admission and applies its penalty.
        auto decision = limiter.admit<FooMessage>(0uz, fooVisitor(1000), start);
        EXPECT_FALSE(decision.admitted());
        EXPECT_TRUE(decision.dropped());
        EXPECT_EQ(decision.tokenCost, 25.0);
        EXPECT_EQ(decision.reason, "foo value is invalid");
        EXPECT_EQ(limiter.size(), 1);

        limiter.onDisconnect(0uz);
        EXPECT_EQ(limiter.size(), 0uz);
    }

    {
        // Try to admit more than 5 connections, should evict old connections.
        for (auto i = 0uz; i < 10; ++i)
        {
            auto decision = limiter.admit<FooMessage>(i, fooVisitor(0), start);
            EXPECT_TRUE(decision.admitted());
            EXPECT_FALSE(decision.dropped());
            EXPECT_EQ(decision.tokenCost, 0.0);
            EXPECT_EQ(limiter.size(), std::min(i + 1, 5uz));
        }
        EXPECT_EQ(limiter.size(), 5uz);
    }

    {
        auto buffer = std::array<uint8_t, 1025>{};  // This costs 10 tokens
        for (auto i = 0uz; i < 6; ++i)
        {
            auto decision = limiter.admitPre<FooMessage>(0uz, buffer, start);
            if (i < 5)
            {
                EXPECT_TRUE(decision.admitted());
            }
            else
            {
                // The sixth one should be dropped because we are out of tokens
                EXPECT_TRUE(decision.dropped());
            }
        }

        auto const refilled = start + std::chrono::seconds{6};
        auto decision = limiter.admitPre<FooMessage>(0uz, buffer, refilled);
        EXPECT_TRUE(decision.admitted());
    }
}

// The same ProtobufMessage spec (scalar `id`, list `items`, object `meta`) admitted through the
// limiter, once per format. Each protobuf payload and its JSON twin below encode the identical
// logical message.

TEST(ConnectionLimiterTests, protobuf_message_admission_over_protobuf)
{
    auto limiter = admission::spec::ConnectionLimiter<int32_t>{
        admission::spec::BucketSettings{.capacity = 1000.0, .refillRatePerSecond = 1.0}, 8};
    auto const now = decltype(limiter)::Clock::now();

    auto admit = [&](std::span<uint8_t const> bytes) {
        return limiter.admit<ProtobufMessage>(
            1, [&](auto check) { return visitProtobuf(bytes, check); }, now);
    };

    // ProtobufMessage { string id = 1; repeated int32 items = 2 [packed]; Meta meta = 3; }
    // Meta { int32 priority = 1; }  — `items` is packed: one field-2 length-delimited blob (tag
    // 0x12) whose payload is the element varints concatenated. The check expands it via
    // visitPackedVarint.

    // { id: "abc", items: [1,2,3], meta: { priority: 4 } } — within every limit.
    auto const ok = std::array<uint8_t, 14>{
        0x0A,
        0x03,
        'a',
        'b',
        'c',  // id = "abc"
        0x12,
        0x03,
        0x01,
        0x02,
        0x03,  // items = [1, 2, 3]  (packed)
        0x1A,
        0x02,
        0x08,
        0x04};  // meta { priority = 4 }
    EXPECT_TRUE(admit(ok).admitted());

    // scalar rule: id = "abcdefghi" (9 > max_id_len 8).
    auto const longId = std::array<uint8_t, 14>{
        0x0A, 0x09, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 0x12, 0x01, 0x01};
    EXPECT_EQ(admit(longId).reason, "id too long");

    // list rule: items = [1,2,3,4] (4 > max_items 3).
    auto const manyItems =
        std::array<uint8_t, 11>{0x0A, 0x03, 'a', 'b', 'c', 0x12, 0x04, 0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(admit(manyItems).reason, "too many items");

    // object rule: meta.priority = 9 (> max_priority 5).
    auto const highPriority = std::array<uint8_t, 12>{
        0x0A, 0x03, 'a', 'b', 'c', 0x12, 0x01, 0x01, 0x1A, 0x02, 0x08, 0x09};
    EXPECT_EQ(admit(highPriority).reason, "priority too high");
}

TEST(ConnectionLimiterTests, protobuf_message_admission_over_json)
{
    auto limiter = admission::spec::ConnectionLimiter<int32_t>{
        admission::spec::BucketSettings{.capacity = 1000.0, .refillRatePerSecond = 1.0}, 8};
    auto const now = decltype(limiter)::Clock::now();

    auto admit = [&](std::string_view json) {
        auto const bytes =
            std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(json.data()), json.size()};
        return limiter.admit<JsonMessage>(
            1, [&](auto check) { return admission::spec::visitJson(bytes, check); }, now);
    };

    EXPECT_TRUE(
        admit(R"JSON({"id": "abc", "items": [1, 2, 3], "meta": {"priority": 4}})JSON").admitted());
    EXPECT_EQ(admit(R"JSON({"id": "abcdefghi", "items": [1]})JSON").reason, "id too long");
    EXPECT_EQ(admit(R"JSON({"id": "abc", "items": [1, 2, 3, 4]})JSON").reason, "too many items");
    EXPECT_EQ(
        admit(R"JSON({"id": "abc", "items": [1], "meta": {"priority": 9}})JSON").reason,
        "priority too high");
}

// The packed fixed-width visitor: a tagless run of 4- or 8-byte little-endian elements, emitted as
// sibling scalars of the given field. Decodes each element (with the schema's signedness),
// propagates the field number, and short-circuits on the first drop.
TEST(ProtobufVisitor, packed_fixed)
{
    using admission::spec::visitPackedFixed;

    // packed sfixed32 [1, 2, -1] — three little-endian 4-byte values.
    auto const f32 = std::array<uint8_t, 12>{
        0x01,
        0x00,
        0x00,
        0x00,  // 1
        0x02,
        0x00,
        0x00,
        0x00,  // 2
        0xFF,
        0xFF,
        0xFF,
        0xFF};  // -1 (sign comes from the sfixed32 element type)
    {
        auto got = std::vector<int64_t>{};
        auto record = [&](VisitEvent const& event) {
            EXPECT_EQ(event.fieldNumber, 7u);
            if (auto const* value = event.as<int64_t>(); value != nullptr)
            {
                got.push_back(*value);
            }
            return AdmissionDecision::admit();
        };
        auto const decision = visitPackedFixed<int32_t>(f32, 7, record);
        EXPECT_TRUE(decision.admitted());
        EXPECT_EQ(got, (std::vector<int64_t>{1, 2, -1}));
    }

    // packed fixed64 [1, 300] — two little-endian 8-byte values (300 = 0x12C).
    auto const f64 = std::array<uint8_t, 16>{
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,  // 1
        0x2C,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00};  // 300
    {
        auto got = std::vector<int64_t>{};
        auto record = [&](VisitEvent const& event) {
            if (auto const* value = event.as<int64_t>(); value != nullptr)
            {
                got.push_back(*value);
            }
            return AdmissionDecision::admit();
        };
        auto const decision = visitPackedFixed<int64_t>(f64, 9, record);
        EXPECT_TRUE(decision.admitted());
        EXPECT_EQ(got, (std::vector<int64_t>{1, 300}));
    }

    // Stops on the first drop: only the elements up to and including the drop are visited.
    {
        auto seen = 0;
        auto stopAtTwo = [&](VisitEvent const& event) {
            ++seen;
            auto const* value = event.as<int64_t>();
            return (value != nullptr and *value == 2) ? AdmissionDecision::drop("stop")
                                                      : AdmissionDecision::admit();
        };
        auto const decision = visitPackedFixed<int32_t>(f32, 7, stopAtTwo);
        EXPECT_TRUE(decision.dropped());
        EXPECT_EQ(seen, 2);  // 1 (admit), 2 (drop) — the third element (-1) is never decoded
    }
}

// The degenerate visitor: no decoding at all, the payload arrives as one opaque Bytes event.
TEST(PassthroughVisitor, emits_whole_payload_as_one_scalar)
{
    using admission::spec::visitPassthrough;

    auto const payload = std::array<uint8_t, 3>{0xDE, 0xAD, 0xBE};
    auto seen = 0;
    auto record = [&](VisitEvent const& event) {
        ++seen;
        EXPECT_EQ(event.kind, EventKind::Bytes);
        EXPECT_TRUE(event.key.empty());
        EXPECT_EQ(event.fieldNumber, std::numeric_limits<uint64_t>::max());
        auto const* value = event.as<std::span<uint8_t const>>();
        EXPECT_NE(value, nullptr);
        EXPECT_EQ(value->size(), payload.size());
        return AdmissionDecision::drop("nope");
    };

    auto const decision = visitPassthrough(std::span<uint8_t const>{payload}, record);
    EXPECT_EQ(seen, 1);
    EXPECT_FALSE(decision.admitted());
    EXPECT_EQ(decision.reason, "nope");
}

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

}  // namespace

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
    }

    // Trailing whitespace is not trailing data.
    EXPECT_TRUE(visitJson(asBytes("{\"a\": 1} \n\t"), admitAll, options).admitted());
}
