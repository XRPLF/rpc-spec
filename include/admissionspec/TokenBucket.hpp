#pragma once

#include <admissionspec/folly/TokenBucket.hpp>

#include <chrono>

namespace admission::spec {

/**
 * @brief A single continuous, lazily-refilled token bucket.
 *
 * A thin adapter over @c folly::TokenBucket that keeps the rest of the admission code free of folly
 * types and of folly's "seconds as double" time convention. The bucket holds up to @c capacity
 * tokens and regains @c refillRatePerSecond tokens per second, never exceeding capacity; an
 * operation of some fractional @c cost succeeds only if at least that many tokens are available at
 * the supplied time point, in which case the tokens are debited.
 *
 * As with the underlying folly type, the caller supplies a monotonic time point on every operation,
 * which keeps the bucket deterministic and testable and lets the owning store pick the clock.
 *
 * @note @c folly::TokenBucket is internally thread-safe (lock-free) for @c consume / @c available,
 * so a single bucket may be shared across threads. The owning per-connection bucket store still
 * needs to synchronize structural changes to its bucket *map* (insertion/eviction) — that is the
 *       deferred D3 concern, separate from the per-bucket arithmetic here.
 *
 * @note A meaningful rate limiter has @c refillRatePerSecond > 0. folly models token count purely
 * as elapsed-time × rate, so a zero rate yields a permanently empty bucket.
 */
class TokenBucket
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    /**
     * @brief Construct a bucket that starts full at @p now.
     *
     * @param capacity Maximum number of tokens the bucket can hold (the burst size); should be > 0.
     * @param refillRatePerSecond Tokens regained per second (the sustained rate); should be > 0.
     * @param now The reference time point the bucket starts from (it holds @p capacity tokens at @p
     * now).
     */
    TokenBucket(double capacity, double refillRatePerSecond, TimePoint now)
        : bucket_{
              refillRatePerSecond,
              capacity,
              fullStartZeroTime(capacity, refillRatePerSecond, toSeconds(now))}
    {
    }

    /**
     * @brief Attempt to consume @p cost tokens as of @p now.
     *
     * @param cost Tokens the operation costs. A non-positive cost always succeeds and debits
     * nothing.
     * @param now Current time point; must be >= the time point passed to the previous call.
     * @return true if the tokens were available and consumed; false otherwise (nothing is debited
     * on failure).
     */
    [[nodiscard]] bool
    tryConsume(double cost, TimePoint now)
    {
        if (cost <= 0.0)
        {
            return true;
        }
        return bucket_.consume(cost, toSeconds(now));
    }

    /**
     * @brief Number of tokens available as of @p now, after accounting for refill.
     *
     * @param now Current time point.
     * @return The available token count, in [0, capacity].
     */
    [[nodiscard]] double
    available(TimePoint now)
    {
        return bucket_.available(toSeconds(now));
    }

    /** @return The bucket capacity (burst size). */
    [[nodiscard]] double
    capacity() const noexcept
    {
        return bucket_.burst();
    }

    /** @return The refill rate in tokens per second. */
    [[nodiscard]] double
    refillRatePerSecond() const noexcept
    {
        return bucket_.rate();
    }

private:
    [[nodiscard]] static double
    toSeconds(TimePoint tp) noexcept
    {
        return std::chrono::duration<double>{tp.time_since_epoch()}.count();
    }

    /** @brief folly derives the token count from (now - zeroTime) * rate, capped at burst. To start
     * full at `nowSeconds`, place zeroTime far enough in the past that the bucket has already
     * filled to capacity.
     */
    [[nodiscard]] static double
    fullStartZeroTime(double capacity, double refillRatePerSecond, double nowSeconds) noexcept
    {
        if (refillRatePerSecond <= 0.0)
        {
            return nowSeconds;
        }
        return nowSeconds - (capacity / refillRatePerSecond);
    }

    folly::TokenBucket bucket_;
};

}  // namespace admission::spec
