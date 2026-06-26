#include <admissionspec/AdmissionSpec.h>
#include <admissionspec/ConnectionLimiter.h>
#include <admissionspec/Types.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <type_traits>

struct FooMessage
{
    size_t foo{0};
};

consteval auto
admissionSpec(std::type_identity<FooMessage>)
{
    return makeSpec<FooMessage>(
               admission::spec::tunable<"max_payload_bytes">(
                   std::uint64_t{64 * 1024}, "admission.foo.max_payload_bytes"),
               admission::spec::tunable<"size_ramp">(
                   admission::spec::ramp(
                       {{.upToBytes = 1024, .cost = 0.5}, {.upToBytes = 64 * 1024, .cost = 10.0}}),
                   "admission.foo.size_ramp"),
               admission::spec::tunable<"min_header_bytes">(
                   std::size_t{4}, "admission.foo.min_header_bytes"),
               admission::spec::tunable<"max_foo_value">(
                   std::size_t{100}, "admission.foo.max_foo_value"))
        .withPreCheck(
            [](std::span<std::byte const> bytes,
               auto const& cfg) -> admission::spec::AdmissionDecision {
                if (bytes.size() < cfg.template get<"min_header_bytes">())
                {
                    return admission::spec::AdmissionDecision::drop(
                        "payload too small to contain a header", 10.0);
                }
                // The user can apply additional token cost here for this admit result.
                return admission::spec::AdmissionDecision::admit();
            })
        .withPostCheck(
            [](FooMessage const& m, auto const& cfg) -> admission::spec::AdmissionDecision {
                if (m.foo > cfg.template get<"max_foo_value">())
                {
                    return admission::spec::AdmissionDecision::drop("foo value is invalid", 25.0);
                }
                // The user can apply additional token cost here for this admit result.
                return admission::spec::AdmissionDecision::admit();
            });
};

TEST(ConnectionLimiterTests, RateLimit)
{
    auto bucketSettings =
        admission::spec::BucketSettings{.capacity = 50, .refillRatePerSecond = 10};
    auto limiter = admission::spec::ConnectionLimiter<std::size_t>{bucketSettings, 5};

    {
        // Verify that a droppable pre condition drops the admission
        auto tooLarge = std::array<std::byte, (64 * 1024) + 1>{};
        auto decision =
            limiter.admitPre<FooMessage>(0uz, tooLarge, std::chrono::steady_clock::now());
        EXPECT_EQ(decision.tokenCost, 10.0);
        EXPECT_EQ(decision.reason, "payload exceeds max bytes for this type");
        EXPECT_FALSE(decision.admitted());
        EXPECT_TRUE(decision.dropped());
        EXPECT_EQ(limiter.size(), 1uz);

        limiter.onDisconnect(0uz);
        EXPECT_EQ(limiter.size(), 0uz);
    }

    {
        // Verify that a droppable post condition drops the admission
        auto foo = FooMessage{};
        foo.foo = 1000;
        auto decision = limiter.admitPost(0uz, foo, std::chrono::steady_clock::now());
        EXPECT_EQ(decision.tokenCost, 25.0);
        EXPECT_EQ(decision.reason, "foo value is invalid");
        EXPECT_FALSE(decision.admitted());
        EXPECT_TRUE(decision.dropped());
        EXPECT_EQ(limiter.size(), 1);

        limiter.onDisconnect(0uz);
        EXPECT_EQ(limiter.size(), 0uz);
    }

    {
        // Try to admit more than 5 connections, should evict old connections.
        for (auto i = 0uz; i < 10; ++i)
        {
            auto foo = FooMessage{};
            auto decision = limiter.admitPost(i, foo, std::chrono::steady_clock::now());
            EXPECT_EQ(decision.tokenCost, 0.0);
            EXPECT_TRUE(decision.admitted());
            EXPECT_FALSE(decision.dropped());
            EXPECT_EQ(limiter.size(), std::min(i + 1, 5uz));
        }
        EXPECT_EQ(limiter.size(), 5uz);
    }

    {
        // Try a "DoS attack"
        auto buffer = std::array<std::byte, 1025>{};  // This cost 10 tokens
        for (auto i = 0uz; i < 6; ++i)
        {
            auto decision =
                limiter.admitPre<FooMessage>(0uz, buffer, std::chrono::steady_clock::now());
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
        std::this_thread::sleep_for(std::chrono::seconds{6});
        auto decision = limiter.admitPre<FooMessage>(0uz, buffer, std::chrono::steady_clock::now());
        // Should succeed after being rate limited and the bucket refilling
        EXPECT_TRUE(decision.admitted());
    }
}
