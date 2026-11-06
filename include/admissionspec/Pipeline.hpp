/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/FrameInfo.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/Rule.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/Standing.hpp>

namespace admission::spec {

/**
 * @brief The outcome of an admission stage: its decision, and where the sender stands after the
 * stage's charge.
 */
struct StageResult
{
    AdmissionDecision decision;
    Standing standing{Standing::Ok};
};

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
            .standing = Standing::Drop};
    }

    static constexpr auto kSpec = admissionSpecFor<T>();
    auto const decision = kSpec.admitHeader(frame, connection, resolvedFor<T>());
    return {.decision = decision, .standing = sink.charge(decision)};
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
 * @param walker Reads the payload, called as `walker(check)`: @ref prescanProtobuf through
 *        @ref protobufVisitor, @ref JsonVisitor, or @ref visitPassthrough.
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
        return {.decision = *refusal, .standing = sink.charge(*refusal)};
    }

    auto check = makeChecker<T>();
    auto const decision = walker(check);
    if (decision.admitted())
    {
        return {
            .decision = decision,
            .standing = decision.tokenCost > 0.0 ? sink.charge(decision) : sink.standing()};
    }

    auto const refusal = AdmissionDecision::drop(
        decision.rule,
        decision.reason,
        kSpec.penaltyFor(decision.penalty),
        kSpec.attributes().relayedByOlderVersions ? 0.0 : decision.tokenCost);
    return {.decision = refusal, .standing = sink.charge(refusal)};
}

}  // namespace admission::spec
