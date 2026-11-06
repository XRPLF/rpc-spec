/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/BucketSettings.hpp>
#include <admissionspec/PenaltyCosts.hpp>
#include <admissionspec/Standing.hpp>
#include <admissionspec/TokenBucket.hpp>
#include <admissionspec/TokenBucketSink.hpp>

#include <chrono>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace admission::spec {

/**
 * @brief One token bucket per connection, handed out as a @ref ChargeSink for each message.
 *
 * For a host with no resource manager of its own: run the stages in Pipeline.hpp with
 * `limiter.sink(conn, now, costs)` as their sink.
 *
 * @tparam ConnId The caller's connection identifier (must be hashable and equality-comparable).
 */
template <typename ConnId>
class ConnectionLimiter
{
    struct BucketEntry
    {
        TokenBucket bucket;
        TokenBucket::TimePoint lastSeen;
    };

    BucketSettings settings_;
    size_t maxConnections_{};
    mutable std::mutex mutex_;
    std::unordered_map<ConnId, BucketEntry> buckets_;

public:
    /**
     * @brief The clock the bucket measures refill against.
     */
    using Clock = TokenBucket::Clock;

    /**
     * @brief A point on @ref Clock.
     */
    using TimePoint = Clock::time_point;

    /**
     * @param settings Resolved per-connection bucket parameters (capacity, refill rate).
     * @param maxConnections Hard cap on the number of tracked connections (bounds memory).
     * @throws std::invalid_argument if a bucket could not be built from @p settings, so a bad
     *         configuration fails at startup rather than when a connection first sends.
     */
    ConnectionLimiter(BucketSettings settings, size_t maxConnections)
        : settings_{settings}, maxConnections_{maxConnections}
    {
        static_cast<void>(
            TokenBucket{settings_.capacity, settings_.refillRatePerSecond, Clock::now()});
    }

    /**
     * @brief A @ref ChargeSink over one connection's bucket.
     *
     * Each call takes the limiter's lock and finds the connection's bucket, creating it if absent,
     * so no reference into the limiter outlives the lock.
     */
    class ConnectionSink
    {
        ConnectionLimiter* limiter_;
        ConnId conn_;
        TimePoint now_;
        PenaltyCosts costs_;

    public:
        ConnectionSink(ConnectionLimiter& limiter, ConnId conn, TimePoint now, PenaltyCosts costs)
            : limiter_{&limiter}, conn_{std::move(conn)}, now_{now}, costs_{costs}
        {
        }

        [[nodiscard]] Standing
        standing()
        {
            auto _ = std::scoped_lock<std::mutex>{limiter_->mutex_};
            return TokenBucketSink{limiter_->touchBucket(conn_, now_), now_, costs_}.standing();
        }

        Standing
        charge(AdmissionDecision const& decision)
        {
            auto _ = std::scoped_lock<std::mutex>{limiter_->mutex_};
            return TokenBucketSink{limiter_->touchBucket(conn_, now_), now_, costs_}.charge(
                decision);
        }
    };

    /**
     * @brief A sink charging @p conn's bucket.
     *
     * @param conn The connection a message arrived on.
     * @param now The time the message is handled.
     * @param costs What each penalty class costs, in tokens.
     * @return The sink. It must not outlive the limiter.
     */
    [[nodiscard]] ConnectionSink
    sink(ConnId const& conn, TimePoint now, PenaltyCosts costs)
    {
        return ConnectionSink{*this, conn, now, costs};
    }

    /**
     * @brief Forget a connection's bucket (call on disconnect).
     *
     * @param conn The connection to drop state for.
     */
    void
    onDisconnect(ConnId const& conn)
    {
        auto _ = std::scoped_lock<std::mutex>{mutex_};
        buckets_.erase(conn);
    }

    /**
     * @brief Evict connections not seen since @p now - @p idleFor.
     *
     * @param now The current time.
     * @param idleFor How long a connection may go unseen before eviction.
     */
    void
    sweepIdle(TimePoint now, std::chrono::steady_clock::duration idleFor)
    {
        auto _ = std::scoped_lock<std::mutex>{mutex_};
        std::erase_if(
            buckets_, [&](auto const& entry) { return now - entry.second.lastSeen >= idleFor; });
    }

    /**
     * @return The number of currently tracked connections.
     */
    [[nodiscard]] size_t
    size() const
    {
        auto _ = std::scoped_lock<std::mutex>{mutex_};
        return buckets_.size();
    }

private:
    /**
     * @brief Find @p conn's bucket, creating it (evicting the oldest if at capacity) if absent,
     * and stamp it as seen at @p now. Caller must hold the lock.
     */
    [[nodiscard]] TokenBucket&
    touchBucket(ConnId const& conn, TimePoint now)
    {
        auto it = buckets_.find(conn);
        if (it == std::end(buckets_))
        {
            if (buckets_.size() >= maxConnections_)
            {
                evictOldest();
            }
            it = buckets_
                     .try_emplace(
                         conn,
                         BucketEntry{
                             TokenBucket{settings_.capacity, settings_.refillRatePerSecond, now},
                             now})
                     .first;
        }
        it->second.lastSeen = now;
        return it->second.bucket;
    }

    void
    evictOldest()
    {
        auto oldest = std::begin(buckets_);
        for (auto it = std::begin(buckets_); it != std::end(buckets_); ++it)
        {
            if (it->second.lastSeen < oldest->second.lastSeen)
            {
                oldest = it;
            }
        }
        if (oldest != std::end(buckets_))
        {
            buckets_.erase(oldest);
        }
    }
};

}  // namespace admission::spec
