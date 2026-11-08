/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/FrameInfo.hpp>
#include <admissionspec/PayloadTotals.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/Rule.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/Standing.hpp>
#include <admissionspec/WalkResult.hpp>

namespace admission::spec {

/**
 * @brief The outcome of an admission stage: its decision, where the sender stands after the
 * stage's charge, and, from the body stage, what its walker counted in the payload.
 */
struct StageResult
{
    AdmissionDecision decision;
    Standing standing{Standing::Ok};
    /**
     * @brief Zero from the header stage, and from a walker that does not count.
     */
    PayloadTotals totals;
};

namespace detail {

// A walker's result as a WalkResult, whichever of the two it returns.
[[nodiscard]] inline WalkResult
asWalkResult(WalkResult result)
{
    return result;
}

[[nodiscard]] inline WalkResult
asWalkResult(AdmissionDecision decision)
{
    return {.decision = decision, .totals = {}};
}

}  // namespace detail

/**
 * @brief The header stage for type @p T: decide from what a frame's header declares, before its
 * payload is read, and charge the decision.
 *
 * A sender already at @ref Standing::Drop is refused unread and not charged again. Otherwise the
 * spec decides (see @ref AdmissionSpec::admitHeader), and the decision's cost and penalty are
 * charged, admitted or not. The host skips a refused frame's bytes, and stops serving a sender
 * whose standing is @ref Standing::Drop.
 *
 * @param frame What the frame's header declares.
 * @param connection The connection the frame arrived on.
 * @param sink Where to charge.
 */
template <typename T, ConnectionView Connection, ChargeSink Sink>
    requires HasAdmissionSpec<T>
[[nodiscard]] StageResult
admitHeader(FrameInfo const& frame, Connection const& connection, Sink& sink)
{
    if (sink.standing() == Standing::Drop)
    {
        return {
            .decision = AdmissionDecision::drop(
                Rule::Sender, "sender is over its limit", PenaltyClass::None),
            .standing = Standing::Drop,
            .totals = {}};
    }

    static constexpr auto kSpec = admissionSpecFor<T>();
    auto const decision = kSpec.admitHeader(frame, connection, resolvedFor<T>());
    return {.decision = decision, .standing = sink.charge(decision), .totals = {}};
}

/**
 * @brief The body stage for type @p T: run the spec's check, behind its repeated-field caps, while
 * a walker reads the payload, before it is parsed, and charge the outcome.
 *
 * It first asks the connection gate (see @ref AdmissionSpec::refuseConnection), so a type the
 * sender may not send is refused unread. For a peer the header stage has already asked it; for
 * RPC, whose method is known only once the body is read, this is where it is settled.
 *
 * The header stage already charged the message's size. A refusal is charged its penalty, capped by
 * the spec (see @ref AdmissionSpec::penaltyFor), with the check's cost unless the spec's type is
 * relayed by older releases. An admission is charged only a cost the check asked for.
 *
 * The result carries what the walker counted in the payload (see @ref PayloadTotals), so a host can
 * observe, for example, unknown-field bytes it does not yet limit.
 *
 * @param walker Reads the payload, called as `walker(check)`, and returns a @ref WalkResult or a
 *        bare @ref AdmissionDecision: @ref prescanProtobuf through @ref protobufVisitor,
 *        @ref JsonVisitor, or @ref visitPassthrough.
 * @param connection The connection the message arrived on.
 * @param sink Where to charge.
 */
template <typename T, typename Walker, ConnectionView Connection, ChargeSink Sink>
    requires HasAdmissionSpec<T>
[[nodiscard]] StageResult
admitBody(Walker&& walker, Connection const& connection, Sink& sink)
{
    static constexpr auto kSpec = admissionSpecFor<T>();
    if (auto const refusal = kSpec.refuseConnection(connection, 0.0))
    {
        return {.decision = *refusal, .standing = sink.charge(*refusal), .totals = {}};
    }

    auto check = makeChecker<T>();
    auto const walked = detail::asWalkResult(walker(check));
    auto const& decision = walked.decision;
    if (decision.admitted())
    {
        return {
            .decision = decision,
            .standing = decision.tokenCost > 0.0 ? sink.charge(decision) : sink.standing(),
            .totals = walked.totals};
    }

    auto const refusal = AdmissionDecision::drop(
        decision.rule,
        decision.reason,
        kSpec.penaltyFor(decision.penalty),
        kSpec.attributes().relayedByOlderVersions ? 0.0 : decision.tokenCost);
    return {.decision = refusal, .standing = sink.charge(refusal), .totals = walked.totals};
}

}  // namespace admission::spec
