#pragma once

#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/TokenBucket.hpp>
#include <admissionspec/Types.hpp>

#include <chrono>
#include <cstddef>
#include <mutex>
#include <span>
#include <unordered_map>

namespace admission::spec {

/**
 * @brief Tracks one token bucket per connection and enforces the admission stages against it.
 *
 * @tparam ConnId The caller's connection identifier (must be hashable and equality-comparable).
 */
template <typename ConnId>
class ConnectionLimiter
{
public:
    using Clock = TokenBucket::Clock;
    using TimePoint = Clock::time_point;

    /**
     * @param settings Resolved per-connection bucket parameters (capacity, refill rate).
     * @param maxConnections Hard cap on the number of tracked connections (bounds memory).
     */
    ConnectionLimiter(BucketSettings settings, std::size_t maxConnections)
        : settings_{settings}, maxConnections_{maxConnections}
    {
    }

    /**
     * @brief Pre-deserialization admission for a message of type @p T arriving on @p conn.
     *
     * Runs @ref preAdmit (hard byte cap + size-ramp cost + pre hook); if admitted, debits the size
     * cost from @p conn's bucket. On a drop, any penalty cost the stage carried is debited too.
     *
     * @return Admit (with the debited cost) or Drop (oversize / pre-hook reject / rate limited).
     */
    template <typename T>
    [[nodiscard]] AdmissionDecision
    admitPre(ConnId const& conn, std::span<std::byte const> payload, TimePoint now)
    {
        auto const decision = preAdmit<T>(payload);
        if (decision.dropped())
        {
            return penalize(conn, decision, now);
        }
        return charge(conn, decision, now);
    }

    /**
     * @brief Streaming admission for a message of type @p T arriving on @p conn.
     *
     * @param visitor Caller-provided traversal, invoked as `visitor(check)`.
     */
    template <typename T, typename Visitor>
    AdmissionDecision
    admit(ConnId const& conn, Visitor visitor, TimePoint now)
    {
        // A fresh, state-carrying checker for this one message; the visitor feeds it each event.
        auto check = admission::spec::makeChecker<T>();

        auto decision = visitor(check);
        if (decision.dropped())
        {
            return penalize(conn, decision, now);
        }
        return charge(conn, decision, now);
    }

    /**
     * @brief Forget a connection's bucket (call on disconnect).
     */
    void
    onDisconnect(ConnId const& conn)
    {
        auto _ = std::scoped_lock<std::mutex>{mutex_};
        state_.buckets.erase(conn);
    }

    /**
     * @brief Evict connections not seen since @p now - @p idleFor.
     */
    void
    sweepIdle(TimePoint now, std::chrono::steady_clock::duration idleFor)
    {
        auto _ = std::scoped_lock<std::mutex>{mutex_};
        std::erase_if(state_.buckets, [&](auto const& entry) {
            return now - entry.second.lastSeen >= idleFor;
        });
    }

    /**
     * @return The number of currently tracked connections.
     */
    [[nodiscard]] std::size_t
    size() const
    {
        auto _ = std::scoped_lock<std::mutex>{mutex_};
        return state_.buckets.size();
    }

private:
    struct BucketEntry
    {
        TokenBucket bucket;
        TimePoint lastSeen;
    };

    struct State
    {
        std::unordered_map<ConnId, BucketEntry> buckets;
    };

    /**
     * @brief Find @p conn's bucket, creating it (evicting the oldest if at capacity) if absent,
     * and stamp it as seen at @p now. Caller must hold @p buckets' lock.
     */
    [[nodiscard]] typename std::unordered_map<ConnId, BucketEntry>::iterator
    touchBucket(std::unordered_map<ConnId, BucketEntry>& buckets, ConnId const& conn, TimePoint now)
    {
        auto it = buckets.find(conn);
        if (it == std::end(buckets))
        {
            if (buckets.size() >= maxConnections_)
            {
                evictOldest(buckets);
            }
            it = buckets
                     .try_emplace(
                         conn,
                         BucketEntry{
                             TokenBucket{settings_.capacity, settings_.refillRatePerSecond, now},
                             now})
                     .first;
        }
        it->second.lastSeen = now;
        return it;
    }

    [[nodiscard]] AdmissionDecision
    charge(ConnId const& conn, AdmissionDecision const& decision, TimePoint now)
    {
        auto _ = std::scoped_lock<std::mutex>{mutex_};
        if (auto it = touchBucket(state_.buckets, conn, now);
            !it->second.bucket.tryConsume(decision.tokenCost, now))
        {
            return AdmissionDecision::drop("connection rate limit exceeded");
        }
        return AdmissionDecision::admit(decision.tokenCost);
    }

    /**
     * @brief Apply a dropped stage's penalty cost to @p conn's bucket and return the drop
     * unchanged. The debit is best-effort: if the bucket lacks the tokens it is left as-is (the
     * connection is already at its limit). A drop is always a drop regardless of bucket state.
     */
    [[nodiscard]] AdmissionDecision
    penalize(ConnId const& conn, AdmissionDecision const& decision, TimePoint now)
    {
        if (decision.tokenCost > 0.0)
        {
            auto _ = std::scoped_lock<std::mutex>{mutex_};
            auto it = touchBucket(state_.buckets, conn, now);
            // Best-effort: debit the penalty if affordable; a drop is a drop regardless.
            (void)it->second.bucket.tryConsume(decision.tokenCost, now);
        }
        return decision;
    }

    static void
    evictOldest(std::unordered_map<ConnId, BucketEntry>& buckets)
    {
        auto oldest = std::begin(buckets);
        for (auto it = std::begin(buckets); it != std::end(buckets); ++it)
        {
            if (it->second.lastSeen < oldest->second.lastSeen)
            {
                oldest = it;
            }
        }
        if (oldest != std::end(buckets))
        {
            buckets.erase(oldest);
        }
    }

    BucketSettings settings_;
    std::size_t maxConnections_;
    mutable std::mutex mutex_;
    State state_;
};

}  // namespace admission::spec
