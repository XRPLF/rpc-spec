#include <admissionspec/TokenBucket.h>
#include <gtest/gtest.h>

#include <chrono>

TEST(TokenBucketTests, TokenBucket)
{
    auto const start = std::chrono::steady_clock::now();

    auto bucket = admission::spec::TokenBucket{50.5, 10.0, start};
    EXPECT_EQ(bucket.available(start), 50.5);
    EXPECT_EQ(bucket.capacity(), 50.5);
    EXPECT_EQ(bucket.refillRatePerSecond(), 10.0);
    EXPECT_TRUE(bucket.tryConsume(50.0, start));
    EXPECT_TRUE(bucket.available(start) < 0.6);
    EXPECT_TRUE(bucket.tryConsume(0.5, start));
    EXPECT_TRUE(bucket.available(start) < 0.1);
    EXPECT_FALSE(bucket.tryConsume(1, start));

    auto const refilled = start + std::chrono::seconds{6};
    EXPECT_EQ(bucket.available(refilled), 50.5);
    EXPECT_TRUE(bucket.tryConsume(50.5, refilled));
    EXPECT_FALSE(bucket.tryConsume(100, refilled));
}
