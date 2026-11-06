/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/Standing.hpp>

namespace admission::spec {

/**
 * @brief A @ref ChargeSink that charges two sinks and reports the worse standing.
 *
 * For a host that keeps a lasting balance per sender and a rate limit per connection, for example
 * `BothSinks<ResourceSink, TokenBucketSink>`.
 */
template <ChargeSink First, ChargeSink Second>
class BothSinks
{
    First* first_;
    Second* second_;

public:
    /**
     * @param first The first sink. It must outlive this one.
     * @param second The second sink. It must outlive this one.
     */
    BothSinks(First& first, Second& second) noexcept : first_{&first}, second_{&second}
    {
    }

    [[nodiscard]] Standing
    standing()
    {
        return worse(first_->standing(), second_->standing());
    }

    /**
     * @brief Charge both sinks, so neither misses a charge when the other refuses.
     */
    Standing
    charge(AdmissionDecision const& decision)
    {
        auto const a = first_->charge(decision);
        auto const b = second_->charge(decision);
        return worse(a, b);
    }
};

}  // namespace admission::spec
