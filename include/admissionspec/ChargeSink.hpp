/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/Standing.hpp>

#include <concepts>

namespace admission::spec {

/**
 * @brief Where the admission stages charge a sender. The host supplies it.
 *
 * The stages hold no balance of their own. `charge` takes a stage's decision and charges the
 * larger of its @c tokenCost and what the sink maps its @c penalty to, so a refusal never costs
 * less than its stage's cost. A host with a resource manager wraps it; a host without one can use
 * @ref TokenBucketSink, or @ref ConnectionLimiter's sinks.
 */
template <typename S>
concept ChargeSink = requires(S& sink, AdmissionDecision const& decision) {
    { sink.standing() } -> std::same_as<Standing>;
    { sink.charge(decision) } -> std::same_as<Standing>;
};

}  // namespace admission::spec
