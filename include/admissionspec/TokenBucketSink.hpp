/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/PenaltyCosts.hpp>
#include <admissionspec/Standing.hpp>
#include <admissionspec/TokenBucket.hpp>

#include <algorithm>

namespace admission::spec {

/**
 * @brief A @ref ChargeSink over one @ref TokenBucket, for a host with no resource manager of its
 * own.
 *
 * Made per message, at the time the message is handled. A charge the bucket cannot cover reports
 * @ref Standing::Drop and takes nothing; the host refuses the message.
 */
class TokenBucketSink
{
    TokenBucket* bucket_;
    TokenBucket::TimePoint now_;
    PenaltyCosts costs_;

public:
    /**
     * @param bucket The bucket. It must outlive the sink.
     * @param now The time the message is handled.
     * @param costs What each penalty class costs, in tokens.
     */
    TokenBucketSink(TokenBucket& bucket, TokenBucket::TimePoint now, PenaltyCosts costs) noexcept
        : bucket_{&bucket}, now_{now}, costs_{costs}
    {
    }

    /**
     * @return @ref Standing::Drop once the bucket is empty, else @ref Standing::Ok.
     */
    [[nodiscard]] Standing
    standing()
    {
        return bucket_->available(now_) > 0.0 ? Standing::Ok : Standing::Drop;
    }

    /**
     * @brief Take the larger of the decision's cost and its penalty from the bucket.
     *
     * @return @ref Standing::Ok if the bucket covered it, else @ref Standing::Drop.
     */
    Standing
    charge(AdmissionDecision const& decision)
    {
        auto const tokens = std::max(decision.tokenCost, costs_(decision.penalty));
        return bucket_->tryConsume(tokens, now_) ? Standing::Ok : Standing::Drop;
    }
};

}  // namespace admission::spec
