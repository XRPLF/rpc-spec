#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/ConnectionLimiter.hpp>
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

using admission::spec::AdmissionDecision;
using admission::spec::EventKind;
using admission::spec::VisitEvent;

namespace {

struct FooMessage
{
    std::int64_t foo{0};
};

consteval auto
admissionSpec(std::type_identity<FooMessage>)
{
    using namespace admission::spec;
    return makeSpec<FooMessage>(
               tunable<"max_payload_bytes">(
                   std::uint64_t{64 * 1024}, "admission.foo.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 1024, .cost = 0.5}, {.upToBytes = 64 * 1024, .cost = 10.0}}),
                   "admission.foo.size_ramp"),
               tunable<"max_foo_value">(std::int64_t{100}, "admission.foo.max_foo_value"))
        .withCheck([](VisitEvent const& e, auto const& cfg) -> AdmissionDecision {
            if (e.fieldNumber == 2)  // `foo`
            {
                if (e.kind != EventKind::Scalar)
                {
                    return AdmissionDecision::drop("foo value is invalid", 25.0);
                }
                if (auto const* v = e.as<std::int64_t>();
                    v != nullptr && *v > cfg.template get<"max_foo_value">())
                {
                    return AdmissionDecision::drop("foo value is invalid", 25.0);
                }
            }
            return AdmissionDecision::admit();
        });
}

[[nodiscard]] auto
fooWalker(std::int64_t fooValue)
{
    return [fooValue](auto check) -> AdmissionDecision {
        return check(VisitEvent{.kind = EventKind::Scalar, .fieldNumber = 2, .value = fooValue});
    };
}

struct CartMessage
{
};

struct CartChecker
{
    bool inItems = false;
    bool inMeta = false;
    std::size_t items = 0;

    // True when this event names the given field, whichever way its format keys things.
    static bool
    is(VisitEvent const& e, std::string_view name, std::int32_t number)
    {
        return e.name == name || e.fieldNumber == number;
    }

    template <typename Cfg>
    AdmissionDecision
    operator()(VisitEvent const& e, Cfg const& cfg)
    {
        switch (e.kind)
        {
            using enum EventKind;
            case BeginArray:
                if (is(e, "items", 2))
                {
                    inItems = true;
                    items = 0;
                }
                return AdmissionDecision::admit();
            case EndArray:
                if (is(e, "items", 2))
                {
                    inItems = false;
                }
                return AdmissionDecision::admit();
            case BeginObject:
                if (is(e, "meta", 3))
                {
                    inMeta = true;
                }
                return AdmissionDecision::admit();
            case EndObject:
                if (is(e, "meta", 3))
                {
                    inMeta = false;
                }
                return AdmissionDecision::admit();
            case BeginMap:
            case EndMap:
                return AdmissionDecision::admit();
            case Scalar:
                // scalar rule: the `id` string may not be too long.
                if (is(e, "id", 1))
                {
                    if (auto const* s = e.as<std::string_view>();
                        s != nullptr && s->size() > cfg.template get<"max_id_len">())
                    {
                        return AdmissionDecision::drop("id too long", 2.0);
                    }
                }
                // list rule: fail on the (max_items+1)th element, as it is decoded.
                if (inItems && ++items > cfg.template get<"max_items">())
                {
                    return AdmissionDecision::drop("too many items", 4.0);
                }
                // object rule: `meta.priority` may not be too high.
                if (inMeta && is(e, "priority", 1))
                {
                    std::cout << "checking priority\n";
                    if (auto const* v = e.as<std::uint64_t>();
                        v != nullptr && *v > cfg.template get<"max_priority">())
                    {
                        std::cout << "checking priority: DROP\n";
                        return AdmissionDecision::drop("priority too high", 8.0);
                    }
                }
                return AdmissionDecision::admit();
        }
        return AdmissionDecision::admit();
    }
};

consteval auto
admissionSpec(std::type_identity<CartMessage>)
{
    using namespace admission::spec;
    return makeSpec<CartMessage>(
               tunable<"max_payload_bytes">(
                   std::uint64_t{64 * 1024}, "admission.cart.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 1024, .cost = 0.5}, {.upToBytes = 64 * 1024, .cost = 4.0}}),
                   "admission.cart.size_ramp"),
               tunable<"max_id_len">(std::size_t{8}, "admission.cart.max_id_len"),
               tunable<"max_items">(std::size_t{3}, "admission.cart.max_items"),
               tunable<"max_priority">(std::uint64_t{5}, "admission.cart.max_priority"))
        .withCheck(CartChecker{});
}

enum class LenKind {
    String,
    PackedInt32,
    Message,
};

[[nodiscard]] bool
readVarint(std::span<std::uint8_t const> bytes, std::size_t& pos, std::uint64_t& out)
{
    auto result = std::uint64_t{};
    auto shift = std::uint64_t{};
    while (pos < bytes.size())
    {
        auto const b = bytes[pos++];
        result |= static_cast<std::uint64_t>(b & 0x7F) << shift;
        if ((b & 0x80) == 0)
        {
            out = result;
            return true;
        }
        shift += 7;
    }
    return false;
}

[[nodiscard]] LenKind
cartShape(std::size_t depth, int field)
{
    if (depth == 0)
    {
        if (field == 2)
        {
            return LenKind::PackedInt32;
        }
        if (field == 3)
        {
            return LenKind::Message;
        }
    }
    return LenKind::String;
}

template <typename Check>
[[nodiscard]] AdmissionDecision
walkProtobuf(std::span<std::uint8_t const> bytes, std::size_t depth, Check& check)
{
    auto pos = std::size_t{};
    while (pos < bytes.size())
    {
        auto tag = std::uint64_t{};
        if (!readVarint(bytes, pos, tag))
        {
            break;
        }
        auto const field = static_cast<std::uint64_t>(tag >> 3);
        auto const wireType = static_cast<std::uint64_t>(tag & 0x07);

        if (wireType == 0)  // varint scalar
        {
            auto v = std::uint64_t{};
            if (!readVarint(bytes, pos, v))
            {
                break;
            }
            if (auto const d =
                    check(VisitEvent{.kind = EventKind::Scalar, .fieldNumber = field, .value = v});
                d.dropped())
            {
                return d;
            }
        }
        else if (wireType == 2)  // length-delimited
        {
            auto len = std::uint64_t{};
            if (!readVarint(bytes, pos, len) || pos + len > bytes.size())
            {
                break;
            }
            auto const body = bytes.subspan(pos, static_cast<std::size_t>(len));
            pos += static_cast<std::size_t>(len);

            switch (cartShape(depth, field))
            {
                using enum LenKind;
                case String: {
                    if (auto const d = check(
                            VisitEvent{
                                .kind = EventKind::Scalar,
                                .fieldNumber = field,
                                .value =
                                    std::string_view{
                                        reinterpret_cast<char const*>(body.data()), body.size()}});
                        d.dropped())
                    {
                        return d;
                    }
                    break;
                }
                case PackedInt32: {
                    if (auto const d =
                            check(VisitEvent{.kind = EventKind::BeginArray, .fieldNumber = field});
                        d.dropped())
                    {
                        return d;
                    }
                    auto p = std::size_t{};
                    while (p < body.size())
                    {
                        auto ev = std::uint64_t{};
                        if (!readVarint(body, p, ev))
                        {
                            break;
                        }
                        if (auto const d = check(
                                VisitEvent{
                                    .kind = EventKind::Scalar, .fieldNumber = field, .value = ev});
                            d.dropped())
                        {
                            return d;
                        }
                    }
                    if (auto const d =
                            check(VisitEvent{.kind = EventKind::EndArray, .fieldNumber = field});
                        d.dropped())
                    {
                        return d;
                    }
                    break;
                }
                case Message: {
                    if (auto const d =
                            check(VisitEvent{.kind = EventKind::BeginObject, .fieldNumber = field});
                        d.dropped())
                    {
                        return d;
                    }
                    if (auto const d = walkProtobuf(body, depth + 1, check); d.dropped())
                    {
                        return d;
                    }
                    if (auto const d =
                            check(VisitEvent{.kind = EventKind::EndObject, .fieldNumber = field});
                        d.dropped())
                    {
                        return d;
                    }
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
    return AdmissionDecision::admit();
}

template <typename Check>
struct JsonWalk
{
    std::string_view s;
    std::size_t i = 0;
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

    std::uint64_t
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
        auto v = std::uint64_t{};
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
walkJson(std::string_view json, Check& check)
{
    auto w = JsonWalk<Check>{json, 0, check};
    return w.value({}, /*topLevel=*/true);
}

}  // namespace

TEST(ConnectionLimiterTests, RateLimit)
{
    auto bucketSettings =
        admission::spec::BucketSettings{.capacity = 50, .refillRatePerSecond = 10};
    auto limiter = admission::spec::ConnectionLimiter<std::size_t>{bucketSettings, 5};

    auto const start = std::chrono::steady_clock::now();

    {
        // Verify that the built-in pre-deserialization size gate drops an oversize payload.
        auto tooLarge = std::array<std::byte, (64 * 1024) + 1>{};
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
        auto decision = limiter.admit<FooMessage>(0uz, fooWalker(1000), start);
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
            auto decision = limiter.admit<FooMessage>(i, fooWalker(0), start);
            EXPECT_TRUE(decision.admitted());
            EXPECT_FALSE(decision.dropped());
            EXPECT_EQ(decision.tokenCost, 0.0);
            EXPECT_EQ(limiter.size(), std::min(i + 1, 5uz));
        }
        EXPECT_EQ(limiter.size(), 5uz);
    }

    {
        // Try a "DoS attack"
        auto buffer = std::array<std::byte, 1025>{};  // This costs 10 tokens
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

// The same Cart spec (scalar `id`, list `items`, object `meta`) admitted through the limiter, once
// per format. Each protobuf payload and its JSON twin below encode the identical logical message.

TEST(ConnectionLimiterTests, CartAdmissionOverProtobuf)
{
    auto limiter = admission::spec::ConnectionLimiter<int>{
        admission::spec::BucketSettings{.capacity = 1000.0, .refillRatePerSecond = 1.0}, 8};
    auto const now = decltype(limiter)::Clock::now();

    auto admit = [&](std::span<std::uint8_t const> bytes) {
        return limiter.admit<CartMessage>(
            /*conn=*/1, [&](auto check) { return walkProtobuf(bytes, 0, check); }, now);
    };

    // { id: "abc", items: [1,2,3], meta: { priority: 4 } } — within every limit.
    auto const ok = std::array<std::uint8_t, 14>{
        0x0A, 0x03, 'a', 'b', 'c', 0x12, 0x03, 0x01, 0x02, 0x03, 0x1A, 0x02, 0x08, 0x04};
    EXPECT_TRUE(admit(ok).admitted());

    // scalar rule: id = "abcdefghi" (9 > max_id_len 8).
    auto const longId = std::array<std::uint8_t, 14>{
        0x0A, 0x09, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 0x12, 0x01, 0x01};
    EXPECT_EQ(admit(longId).reason, "id too long");

    // list rule: items = [1,2,3,4] (4 > max_items 3).
    auto const manyItems =
        std::array<std::uint8_t, 11>{0x0A, 0x03, 'a', 'b', 'c', 0x12, 0x04, 0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(admit(manyItems).reason, "too many items");

    // object rule: meta.priority = 9 (> max_priority 5).
    auto const highPriority = std::array<std::uint8_t, 14>{
        0x0A, 0x03, 'a', 'b', 'c', 0x12, 0x03, 0x01, 0x02, 0x03, 0x1A, 0x02, 0x08, 0x09};
    EXPECT_EQ(admit(highPriority).reason, "priority too high");
}

TEST(ConnectionLimiterTests, CartAdmissionOverJson)
{
    auto limiter = admission::spec::ConnectionLimiter<int>{
        admission::spec::BucketSettings{.capacity = 1000.0, .refillRatePerSecond = 1.0}, 8};
    auto const now = decltype(limiter)::Clock::now();

    auto admit = [&](std::string_view json) {
        return limiter.admit<CartMessage>(
            /*conn=*/1, [&](auto check) { return walkJson(json, check); }, now);
    };

    EXPECT_TRUE(admit(R"({"id":"abc","items":[1,2,3],"meta":{"priority":4}})").admitted());
    EXPECT_EQ(admit(R"({"id":"abcdefghi","items":[1]})").reason, "id too long");
    EXPECT_EQ(admit(R"({"id":"abc","items":[1,2,3,4]})").reason, "too many items");
    EXPECT_EQ(
        admit(R"({"id":"abc","items":[1],"meta":{"priority":9}})").reason, "priority too high");
}
