#include <admissionspec/BucketSettings.hpp>
#include <admissionspec/ConnectionLimiter.hpp>
#include <admissionspec/TokenBucket.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <stdexcept>

TEST(TokenBucketTests, token_bucket)
{
    auto const start = std::chrono::steady_clock::now();

    auto bucket = admission::spec::TokenBucket{50.5, 10.0, start};
    EXPECT_NEAR(bucket.available(start), 50.5, 0.1);
    EXPECT_EQ(bucket.capacity(), 50.5);
    EXPECT_EQ(bucket.refillRatePerSecond(), 10.0);
    EXPECT_TRUE(bucket.tryConsume(50.0, start));
    EXPECT_TRUE(bucket.available(start) < 0.6);
    EXPECT_TRUE(bucket.tryConsume(0.3, start));
    EXPECT_TRUE(bucket.available(start) < 0.4);
    EXPECT_FALSE(bucket.tryConsume(1, start));

    auto const refilled = start + std::chrono::seconds{6};
    EXPECT_EQ(bucket.available(refilled), 50.5);
    EXPECT_TRUE(bucket.tryConsume(50.5, refilled));
    EXPECT_FALSE(bucket.tryConsume(100, refilled));
}

TEST(TokenBucketTests, refuses_settings_that_cannot_hold_tokens)
{
    using admission::spec::TokenBucket;
    auto const now = std::chrono::steady_clock::now();
    auto const nan = std::numeric_limits<double>::quiet_NaN();

    // The token count comes from elapsed time times the rate, so a bucket that never refills could
    // never start full.
    EXPECT_THROW(TokenBucket(10.0, 0.0, now), std::invalid_argument);
    EXPECT_THROW(TokenBucket(10.0, -1.0, now), std::invalid_argument);
    EXPECT_THROW(TokenBucket(10.0, nan, now), std::invalid_argument);
    EXPECT_THROW(TokenBucket(0.0, 1.0, now), std::invalid_argument);
    EXPECT_THROW(TokenBucket(nan, 1.0, now), std::invalid_argument);
}

TEST(TokenBucketTests, a_limiter_refuses_bad_settings_when_it_is_built)
{
    using Limiter = admission::spec::ConnectionLimiter<int>;
    EXPECT_THROW(
        Limiter(admission::spec::BucketSettings{.capacity = 10.0, .refillRatePerSecond = 0.0}, 4),
        std::invalid_argument);
    EXPECT_NO_THROW(
        Limiter(admission::spec::BucketSettings{.capacity = 10.0, .refillRatePerSecond = 1.0}, 4));
}
