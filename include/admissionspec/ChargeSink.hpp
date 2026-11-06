/** @file */
#pragma once

#include <admissionspec/Frame.hpp>
#include <admissionspec/TokenBucket.hpp>
#include <admissionspec/Types.hpp>

#include <algorithm>
#include <concepts>
#include <memory>
#include <string_view>

namespace admission::spec {

/**
 * @brief What an admission stage asks the host to charge.
 *
 * The sink charges the larger of @c cost and what it maps @c penalty to, so a refusal never costs
 * less than its stage's cost.
 */
struct ChargeRequest
{
    /**
     * @brief The stage's cost, in the host's units.
     */
    double cost{};

    /**
     * @brief How a refusal is penalized. @c None for an admission.
     */
    PenaltyClass penalty{PenaltyClass::None};

    /**
     * @brief Why, for the host's log.
     */
    std::string_view reason;
};

/**
 * @brief The request a stage's decision asks to have charged.
 */
[[nodiscard]] constexpr ChargeRequest
chargeFor(AdmissionDecision const& decision) noexcept
{
    return {.cost = decision.tokenCost, .penalty = decision.penalty, .reason = decision.reason};
}

/**
 * @brief Where the admission stages charge a sender. The host supplies it.
 *
 * The stages hold no balance of their own. A host with a resource manager wraps it; a host without
 * one can use @ref TokenBucketSink.
 */
template <typename S>
concept ChargeSink = requires(S& sink, ChargeRequest const& request) {
    { sink.standing() } -> std::same_as<Standing>;
    { sink.charge(request) } -> std::same_as<Standing>;
};

/**
 * @brief The cost of each penalty class, in a sink's units.
 *
 * There are no defaults: what a class costs is the host's policy.
 */
struct PenaltyCosts
{
    double trivial{};
    double useless{};
    double invalid{};
    double malformed{};

    /**
     * @return The cost of @p penalty.
     */
    [[nodiscard]] constexpr double
    operator()(PenaltyClass penalty) const noexcept
    {
        switch (penalty)
        {
            case PenaltyClass::None:
                return 0.0;
            case PenaltyClass::Trivial:
                return trivial;
            case PenaltyClass::Useless:
                return useless;
            case PenaltyClass::Invalid:
                return invalid;
            case PenaltyClass::Malformed:
                return malformed;
        }
        return malformed;
    }
};

/**
 * @brief A @ref ChargeSink over one connection's @ref TokenBucket, for a host with no resource
 * manager of its own.
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
     * @param bucket The connection's bucket. It must outlive the sink.
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
     * @brief Take the larger of the request's cost and its penalty from the bucket.
     *
     * @return @ref Standing::Ok if the bucket covered it, else @ref Standing::Drop.
     */
    Standing
    charge(ChargeRequest const& request)
    {
        auto const tokens = std::max(request.cost, costs_(request.penalty));
        return bucket_->tryConsume(tokens, now_) ? Standing::Ok : Standing::Drop;
    }
};

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
    charge(ChargeRequest const& request)
    {
        auto const a = first_->charge(request);
        auto const b = second_->charge(request);
        return worse(a, b);
    }
};

/**
 * @brief A type-erased, non-owning @ref ChargeSink, for a host that picks its sink at run time.
 *
 * The stages take `ChargeSink auto&`, so a host with one sink type pays no indirect call; this is
 * the alternative when the type varies.
 */
class AnyChargeSink
{
    void* sink_;
    Standing (*standing_)(void*);
    Standing (*charge_)(void*, ChargeRequest const&);

public:
    /**
     * @param sink The sink to forward to. It must outlive this one.
     */
    template <ChargeSink S>
        requires(not std::same_as<S, AnyChargeSink>)
    explicit AnyChargeSink(S& sink) noexcept
        : sink_{std::addressof(sink)}
        , standing_{[](void* s) { return static_cast<S*>(s)->standing(); }}
        , charge_{[](void* s, ChargeRequest const& r) { return static_cast<S*>(s)->charge(r); }}
    {
    }

    [[nodiscard]] Standing
    standing()
    {
        return standing_(sink_);
    }

    Standing
    charge(ChargeRequest const& request)
    {
        return charge_(sink_, request);
    }
};

}  // namespace admission::spec
