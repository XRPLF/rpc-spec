#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/BucketSettings.hpp>
#include <admissionspec/ConnectionLimiter.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/FrameInfo.hpp>
#include <admissionspec/JsonVisitor.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/PenaltyCosts.hpp>
#include <admissionspec/Pipeline.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/Standing.hpp>
#include <admissionspec/Tunable.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

using namespace admission::spec;

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

// Admits every frame on its own merits: the tests below are about the limiter's buckets.
struct OpenConnection
{
    [[nodiscard]] static bool
    inCluster()
    {
        return false;
    }

    [[nodiscard]] static bool
    hasFeature(FeatureId)
    {
        return false;
    }

    [[nodiscard]] static bool
    compressionNegotiated()
    {
        return false;
    }
};

static_assert(ConnectionView<OpenConnection>);

constexpr auto kCosts = PenaltyCosts{.trivial = 1, .useless = 10, .invalid = 25, .malformed = 50};

[[nodiscard]] FrameInfo
frameOf(uint64_t bytes)
{
    return {.wireBytes = bytes, .decodedBytes = bytes};
}

}  // namespace

TEST(ConnectionLimiterTests, rate_limit)
{
    auto limiter =
        ConnectionLimiter<size_t>{BucketSettings{.capacity = 50, .refillRatePerSecond = 10}, 5};
    auto const start = std::chrono::steady_clock::now();

    {
        // The header stage refuses an oversize payload, at the ramp price of its size.
        auto sink = limiter.sink(0uz, start, kCosts);
        auto const result =
            admitHeader<FooMessage>(frameOf((64 * 1024) + 1), OpenConnection{}, sink);
        EXPECT_TRUE(result.decision.dropped());
        EXPECT_EQ(result.decision.reason, "payload exceeds max bytes for this type");
        EXPECT_EQ(result.decision.penalty, PenaltyClass::Useless);
        EXPECT_EQ(result.decision.tokenCost, 10.0);
        EXPECT_EQ(limiter.size(), 1uz);

        limiter.onDisconnect(0uz);
        EXPECT_EQ(limiter.size(), 0uz);
    }

    {
        // The body stage runs the spec's check and charges its refusal.
        auto sink = limiter.sink(0uz, start, kCosts);
        auto const result = admitBody<FooMessage>(fooVisitor(1000), sink);
        EXPECT_TRUE(result.decision.dropped());
        EXPECT_EQ(result.decision.reason, "foo value is invalid");
        EXPECT_EQ(result.decision.tokenCost, 25.0);
        EXPECT_EQ(limiter.size(), 1uz);

        limiter.onDisconnect(0uz);
        EXPECT_EQ(limiter.size(), 0uz);
    }

    {
        // More connections than the cap evicts the oldest.
        for (auto i = 0uz; i < 10; ++i)
        {
            auto sink = limiter.sink(i, start, kCosts);
            EXPECT_TRUE(admitBody<FooMessage>(fooVisitor(0), sink).decision.admitted());
            EXPECT_EQ(limiter.size(), std::min(i + 1, 5uz));
        }
        EXPECT_EQ(limiter.size(), 5uz);
    }

    {
        // Each 1025-byte frame costs 10 tokens, so the sixth is refused unread once the bucket is
        // empty, and the bucket refills with time.
        for (auto i = 0uz; i < 6; ++i)
        {
            auto sink = limiter.sink(0uz, start, kCosts);
            auto const result = admitHeader<FooMessage>(frameOf(1025), OpenConnection{}, sink);
            EXPECT_EQ(result.decision.admitted(), i < 5);
        }
        EXPECT_EQ(limiter.sink(0uz, start, kCosts).standing(), Standing::Drop);

        auto refilled = limiter.sink(0uz, start + std::chrono::seconds{6}, kCosts);
        EXPECT_TRUE(
            admitHeader<FooMessage>(frameOf(1025), OpenConnection{}, refilled).decision.admitted());
    }
}

TEST(ConnectionLimiterTests, json_message_admission)
{
    auto limiter = ConnectionLimiter<int32_t>{
        BucketSettings{.capacity = 1000.0, .refillRatePerSecond = 1.0}, 8};
    auto const now = std::chrono::steady_clock::now();

    auto admit = [&](std::string_view json) {
        auto const bytes =
            std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(json.data()), json.size()};
        auto sink = limiter.sink(1, now, kCosts);
        return admitBody<JsonMessage>([&](auto& check) { return visitJson(bytes, check); }, sink)
            .decision;
    };

    EXPECT_TRUE(
        admit(R"JSON({"id": "abc", "items": [1, 2, 3], "meta": {"priority": 4}})JSON").admitted());
    EXPECT_EQ(admit(R"JSON({"id": "abcdefghi", "items": [1]})JSON").reason, "id too long");
    EXPECT_EQ(admit(R"JSON({"id": "abc", "items": [1, 2, 3, 4]})JSON").reason, "too many items");
    EXPECT_EQ(
        admit(R"JSON({"id": "abc", "items": [1], "meta": {"priority": 9}})JSON").reason,
        "priority too high");
}
