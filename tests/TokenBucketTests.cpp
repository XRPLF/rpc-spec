#include <admissionspec/TokenBucket.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

TEST(TokenBucketTests, TokenBucket)
{
    auto bucket = admission::spec::TokenBucket{50.5, 10.0, std::chrono::steady_clock::now()};
    EXPECT_EQ(bucket.available(std::chrono::steady_clock::now()), 50.5);
    EXPECT_EQ(bucket.capacity(), 50.5);
    EXPECT_EQ(bucket.refillRatePerSecond(), 10.0);
    EXPECT_TRUE(bucket.tryConsume(50.0, std::chrono::steady_clock::now()));
    EXPECT_TRUE(bucket.available(std::chrono::steady_clock::now()) < 0.6);
    EXPECT_TRUE(bucket.tryConsume(0.5, std::chrono::steady_clock::now()));
    EXPECT_TRUE(bucket.available(std::chrono::steady_clock::now()) < 0.1);
    EXPECT_FALSE(bucket.tryConsume(1, std::chrono::steady_clock::now()));
    std::this_thread::sleep_for(std::chrono::seconds{6});
    EXPECT_EQ(bucket.available(std::chrono::steady_clock::now()), 50.5);
    EXPECT_FALSE(bucket.tryConsume(100, std::chrono::steady_clock::now()));
}
