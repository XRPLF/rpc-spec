#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/ConnectionLimiter.hpp>
#include <admissionspec/ProtobufVisitor.hpp>
#include <admissionspec/Types.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
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
        .withCheck([](VisitEvent const& e, auto const& cfg) -> AdmissionDecision {
            if (e.fieldNumber == 2)  // `foo`
            {
                if (e.kind != EventKind::Scalar)
                {
                    return AdmissionDecision::drop("foo value is invalid", 25.0);
                }
                if (auto const* v = e.as<int64_t>();
                    v != nullptr && *v > cfg.template get<"max_foo_value">())
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
        return check(VisitEvent{.kind = EventKind::Scalar, .fieldNumber = 2, .value = fooValue});
    };
}

struct ProtobufMessage
{
};

// Drives PROTOBUF only. Every field arrives as a Scalar keyed by field number; a length-delimited
// field's `value` carries a byte span, which the check re-enters with the matching walker
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
    operator()(VisitEvent const& e, Cfg const& cfg)
    {
        auto self = [&](VisitEvent const& ev) { return (*this)(ev, cfg); };

        if (inMeta)
        {
            // Inside `meta`: priority is field 1.
            if (e.fieldNumber == 1)
            {
                if (auto const* v = e.as<int64_t>();
                    v != nullptr && *v > cfg.template get<"max_priority">())
                {
                    return AdmissionDecision::drop("priority too high", 8.0);
                }
            }
            return AdmissionDecision::admit();
        }

        switch (e.fieldNumber)
        {
            case 1:  // id: a length-delimited string
            {
                auto const* b = e.as<std::span<uint8_t const>>();
                auto const id = b != nullptr
                    ? std::string_view{reinterpret_cast<char const*>(b->data()), b->size()}
                    : std::string_view{};
                if (id.size() > cfg.template get<"max_id_len">())
                {
                    return AdmissionDecision::drop("id too long", 2.0);
                }
                return AdmissionDecision::admit();
            }
            case 2:  // items: packed repeated int32 (a byte span) — re-enter, counting each element
            {
                if (auto const* body = e.as<std::span<uint8_t const>>(); body != nullptr)
                {
                    return admission::spec::visitPackedVarint(*body, e.fieldNumber, self);
                }
                if (++items > cfg.template get<"max_items">())
                {
                    return AdmissionDecision::drop("too many items", 4.0);
                }
                return AdmissionDecision::admit();
            }
            case 3:  // meta: a sub-message — descend, bracketing with `inMeta`
            {
                if (auto const* body = e.as<std::span<uint8_t const>>(); body != nullptr)
                {
                    inMeta = true;
                    auto const d = admission::spec::visitProtobuf(*body, self);
                    inMeta = false;
                    return d;
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
    operator()(VisitEvent const& e, Cfg const& cfg)
    {
        switch (e.kind)
        {
            using enum EventKind;
            case BeginArray:
                if (e.name == "items")
                {
                    inItems = true;
                    items = 0;
                }
                return AdmissionDecision::admit();
            case EndArray:
                if (e.name == "items")
                {
                    inItems = false;
                }
                return AdmissionDecision::admit();
            case BeginObject:
                if (e.name == "meta")
                {
                    inMeta = true;
                }
                return AdmissionDecision::admit();
            case EndObject:
                if (e.name == "meta")
                {
                    inMeta = false;
                }
                return AdmissionDecision::admit();
            case Scalar:
                if (e.name == "id")
                {
                    if (auto const* s = e.as<std::string_view>();
                        s != nullptr && s->size() > cfg.template get<"max_id_len">())
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
                if (inMeta && e.name == "priority")
                {
                    if (auto const* v = e.as<int64_t>();
                        v != nullptr && *v > cfg.template get<"max_priority">())
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

/**
 * @brief A sax like parser just for testing purposes in this test harness.  This
 *        is not a true sax parser.
 */
template <typename Check>
struct JsonVisitor
{
    std::string_view s;
    size_t i{};
    Check& check;

    void
    skipWs()
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r'))
        {
            ++i;
        }
    }

    std::string_view
    parseString()  // s[i] == '"'
    {
        ++i;
        auto const start = i;
        while (i < s.size() && s[i] != '"')
        {
            ++i;
        }
        auto const r = s.substr(start, i - start);
        if (i < s.size())
        {
            ++i;
        }
        return r;
    }

    int64_t
    parseInt()
    {
        auto const start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+'))
        {
            ++i;
        }
        while (i < s.size() && s[i] >= '0' && s[i] <= '9')
        {
            ++i;
        }
        auto v = int64_t{};
        std::from_chars(s.data() + start, s.data() + i, v);
        return v;
    }

    AdmissionDecision
    value(std::string_view name, bool topLevel)
    {
        skipWs();
        if (i >= s.size())
        {
            return AdmissionDecision::admit();
        }
        auto const c = s[i];
        if (c == '{')
        {
            return object(name, topLevel);
        }
        if (c == '[')
        {
            return array(name);
        }
        auto e = VisitEvent{.kind = EventKind::Scalar, .name = name};
        if (c == '"')
        {
            e.value = parseString();
        }
        else
        {
            e.value = parseInt();
        }
        return check(e);
    }

    AdmissionDecision
    object(std::string_view name, bool topLevel)
    {
        ++i;            // '{'
        if (!topLevel)  // the top-level object is the message itself — no wrapper event
        {
            if (auto const d = check(VisitEvent{.kind = EventKind::BeginObject, .name = name});
                d.dropped())
            {
                return d;
            }
        }
        skipWs();
        while (i < s.size() && s[i] != '}')
        {
            skipWs();
            auto const key = parseString();
            skipWs();
            if (i < s.size() && s[i] == ':')
            {
                ++i;
            }
            if (auto const d = value(key, false); d.dropped())
            {
                return d;
            }
            skipWs();
            if (i < s.size() && s[i] == ',')
            {
                ++i;
            }
            skipWs();
        }
        if (i < s.size())
        {
            ++i;  // '}'
        }
        if (!topLevel)
        {
            if (auto const d = check(VisitEvent{.kind = EventKind::EndObject, .name = name});
                d.dropped())
            {
                return d;
            }
        }
        return AdmissionDecision::admit();
    }

    AdmissionDecision
    array(std::string_view name)
    {
        ++i;  // '['
        if (auto const d = check(VisitEvent{.kind = EventKind::BeginArray, .name = name});
            d.dropped())
        {
            return d;
        }
        skipWs();
        while (i < s.size() && s[i] != ']')
        {
            if (auto const d = value({}, false); d.dropped())
            {
                return d;
            }
            skipWs();
            if (i < s.size() && s[i] == ',')
            {
                ++i;
            }
            skipWs();
        }
        if (i < s.size())
        {
            ++i;  // ']'
        }
        return check(VisitEvent{.kind = EventKind::EndArray, .name = name});
    }
};

template <typename Check>
[[nodiscard]] AdmissionDecision
visitJson(std::string_view json, Check& check)
{
    auto w = JsonVisitor<Check>{json, 0, check};
    return w.value({}, true);
}

}  // namespace

TEST(ConnectionLimiterTests, RateLimit)
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
        // Try a "DoS attack"
        auto buffer = std::array<uint8_t, 1025>{};  // This costs 10 tokens
        for (auto i = 0uz; i < 6; ++i)
        {
            auto decision = limiter.admitPre<FooMessage>(0uz, buffer, start);
            if (i < 5)
            {
                // These should be admitted
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
        // Should succeed after being rate limited and the bucket refilling
        EXPECT_TRUE(decision.admitted());
    }
}

// The same ProtobufMessage spec (scalar `id`, list `items`, object `meta`) admitted through the
// limiter, once per format. Each protobuf payload and its JSON twin below encode the identical
// logical message.

TEST(ConnectionLimiterTests, ProtobufMessageAdmissionOverProtobuf)
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

TEST(ConnectionLimiterTests, ProtobufMessageAdmissionOverJson)
{
    auto limiter = admission::spec::ConnectionLimiter<int32_t>{
        admission::spec::BucketSettings{.capacity = 1000.0, .refillRatePerSecond = 1.0}, 8};
    auto const now = decltype(limiter)::Clock::now();

    auto admit = [&](std::string_view json) {
        return limiter.admit<JsonMessage>(
            1, [&](auto check) { return visitJson(json, check); }, now);
    };

    EXPECT_TRUE(admit(R"({"id":"abc","items":[1,2,3],"meta":{"priority":4}})").admitted());
    EXPECT_EQ(admit(R"({"id":"abcdefghi","items":[1]})").reason, "id too long");
    EXPECT_EQ(admit(R"({"id":"abc","items":[1,2,3,4]})").reason, "too many items");
    EXPECT_EQ(
        admit(R"({"id":"abc","items":[1],"meta":{"priority":9}})").reason, "priority too high");
}

// The packed fixed-width visitor: a tagless run of 4- or 8-byte little-endian elements, emitted as
// sibling scalars of the given field. Decodes each element (with the schema's signedness),
// propagates the field number, and short-circuits on the first drop.
TEST(ProtobufVisitor, PackedFixed)
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
        auto record = [&](VisitEvent const& e) {
            EXPECT_EQ(e.fieldNumber, 7u);
            if (auto const* v = e.as<int64_t>(); v != nullptr)
            {
                got.push_back(*v);
            }
            return AdmissionDecision::admit();
        };
        auto const d = visitPackedFixed<int32_t>(f32, 7, record);
        EXPECT_TRUE(d.admitted());
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
        auto record = [&](VisitEvent const& e) {
            if (auto const* v = e.as<int64_t>(); v != nullptr)
            {
                got.push_back(*v);
            }
            return AdmissionDecision::admit();
        };
        auto const d = visitPackedFixed<int64_t>(f64, 9, record);
        EXPECT_TRUE(d.admitted());
        EXPECT_EQ(got, (std::vector<int64_t>{1, 300}));
    }

    // Stops on the first drop: only the elements up to and including the drop are visited.
    {
        auto seen = 0;
        auto stopAtTwo = [&](VisitEvent const& e) {
            ++seen;
            auto const* v = e.as<int64_t>();
            return (v != nullptr && *v == 2) ? AdmissionDecision::drop("stop")
                                             : AdmissionDecision::admit();
        };
        auto const d = visitPackedFixed<int32_t>(f32, 7, stopAtTwo);
        EXPECT_TRUE(d.dropped());
        EXPECT_EQ(seen, 2);  // 1 (admit), 2 (drop) — the third element (-1) is never decoded
    }
}
