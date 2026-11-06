/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/Standing.hpp>

#include <tuple>

namespace admission::spec {

/**
 * @brief A @ref ChargeSink that charges every one of its sinks and reports the worst standing.
 *
 * For a host that keeps a lasting balance per sender and a rate limit per connection, for example
 * `AllSinks<ResourceSink, TokenBucketSink>`, with room for more, such as a sink that only counts.
 */
template <ChargeSink... Sinks>
    requires(sizeof...(Sinks) > 0)
class AllSinks
{
    std::tuple<Sinks*...> sinks_;

public:
    /**
     * @param sinks The sinks, charged in this order. Each must outlive this one.
     */
    explicit AllSinks(Sinks&... sinks) noexcept : sinks_{&sinks...}
    {
    }

    [[nodiscard]] Standing
    standing()
    {
        return std::apply(
            [](auto*... sink) {
                auto result = Standing::Ok;
                ((result = worse(result, sink->standing())), ...);
                return result;
            },
            sinks_);
    }

    /**
     * @brief Charge every sink, so none misses a charge when another refuses.
     */
    Standing
    charge(AdmissionDecision const& decision)
    {
        return std::apply(
            [&decision](auto*... sink) {
                auto result = Standing::Ok;
                ((result = worse(result, sink->charge(decision))), ...);
                return result;
            },
            sinks_);
    }
};

}  // namespace admission::spec
