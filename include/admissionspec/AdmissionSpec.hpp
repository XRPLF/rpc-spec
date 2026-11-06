/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/FrameInfo.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/ResolvedTunables.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/SpecAttributes.hpp>
#include <admissionspec/VisitEvent.hpp>

#include <concepts>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>

namespace admission::spec {

/**
 * @brief A streaming check invoked by a caller-provided visitor as it emits @ref
 * admission::spec::VisitEvent events.
 */
template <typename F, typename Resolved>
concept SomeCheck = requires(F check, VisitEvent const& event, Resolved const& cfg) {
    { check(event, cfg) } -> std::same_as<AdmissionDecision>;
};

/**
 * Sentinel for an unattached hook slot.
 */
struct NoHook
{
};

/**
 * @brief The declared admission policy for a message type @p T.
 *
 * A spec holds three things:
 * - named @ref Tunable values, which configuration may override. Two are required:
 *   @c "max_payload_bytes" (the cap on either declared size) and @c "size_ramp" (the price by
 *   decoded size); `makeSpec` checks both are present;
 * - @ref SpecAttributes, the type's fixed facts: its type id, whether only the cluster may send it,
 *   a required feature, whether older releases relay it as received, and repeated-field caps;
 * - optionally, a check: a callable invoked with each @ref VisitEvent while the payload is walked,
 *   which keeps any state it needs and may refuse the message before it is parsed.
 *
 * A spec is built at compile time and lives as a @c static @c constexpr object.
 *
 * @code
 * consteval auto admissionSpec(std::type_identity<MyMessage>)
 * {
 *     return makeSpec<MyMessage>(
 *                tunable<"max_payload_bytes">(uint64_t{64 * 1024},
 * "admission.my.max_payload_bytes"), tunable<"size_ramp">(ramp({{1024, 0.5}, {64 * 1024, 4.0}}),
 * "admission.my.size_ramp"), tunable<"max_priority">(int64_t{9}, "admission.my.max_priority"))
 *         .withAttributes({.typeId = 7, .repeated = repeatedCaps({{.fieldNumber = 3, .maxEntries =
 * 100}})}) .withCheck([](VisitEvent const& event, auto const& cfg) { if (auto const* p =
 * event.as<int64_t>(); event.depth == 0 and event.fieldNumber == 1 and p != nullptr and *p >
 * cfg.template get<"max_priority">())
 *             {
 *                 return AdmissionDecision::drop("priority too high", PenaltyClass::Invalid);
 *             }
 *             return AdmissionDecision::admit();
 *         });
 * }
 * @endcode
 *
 * The stages that apply it are in Pipeline.hpp: @ref admitHeader decides from the frame header,
 * @ref admitBody runs the check while a walker reads the payload, and both charge a host-supplied
 * @ref ChargeSink.
 */
template <typename T, typename TunablesTuple, typename Check = NoHook>
class AdmissionSpec
{
    TunablesTuple tunables_;
    Check check_;
    SpecAttributes attributes_;

public:
    /**
     * @brief The message type this spec governs.
     */
    using Type = T;

    /**
     * @brief The resolved tunable bag this spec evaluates against.
     */
    using Resolved = ResolvedTunablesOfT<TunablesTuple>;

    /**
     * @brief Construct a @ref AdmissionSpec.
     *
     * @param tunables The declared tunables.
     * @param check The streaming check, or NoHook when none is attached.
     * @param attributes The type's fixed admission facts.
     */
    consteval AdmissionSpec(TunablesTuple tunables, Check check, SpecAttributes attributes = {})
        : tunables_{std::move(tunables)}, check_{std::move(check)}, attributes_{attributes}
    {
    }

    /**
     * @brief Attach a streaming check; returns the updated spec.
     *
     * @tparam C The check callable, invoked per @ref VisitEvent.
     * @param check The check to attach.
     * @return A copy of this spec carrying @p check.
     */
    template <typename C>
        requires SomeCheck<C, Resolved>
    [[nodiscard]] consteval auto
    withCheck(C check) const
    {
        return AdmissionSpec<T, TunablesTuple, C>{tunables_, check, attributes_};
    }

    /**
     * @brief Attach the type's fixed admission facts; returns the updated spec.
     *
     * @param attributes The facts to attach.
     * @return A copy of this spec carrying @p attributes.
     */
    [[nodiscard]] consteval AdmissionSpec
    withAttributes(SpecAttributes attributes) const
    {
        return AdmissionSpec{tunables_, check_, attributes};
    }

    /**
     * @brief The type's fixed admission facts.
     */
    [[nodiscard]] constexpr SpecAttributes const&
    attributes() const noexcept
    {
        return attributes_;
    }

    /**
     * @brief The penalty a refusal of this type carries, given the class the failed rule asks for.
     *
     * A type an older release passes on as received is refused at @ref PenaltyClass::Trivial at
     * most, so an honest relay is not penalized for what it did not write.
     */
    [[nodiscard]] constexpr PenaltyClass
    penaltyFor(PenaltyClass requested) const noexcept
    {
        if (attributes_.relayedByOlderVersions and requested > PenaltyClass::Trivial)
        {
            return PenaltyClass::Trivial;
        }
        return requested;
    }

    /**
     * @brief The spec's declared tunables.
     *
     * @return A reference to the tunable tuple.
     */
    [[nodiscard]] constexpr TunablesTuple const&
    tunables() const noexcept
    {
        return tunables_;
    }

    /**
     * @brief Build a @ref ResolvedTunables from the spec's defaults (no config overrides).
     *
     * @return The resolved tunable bag.
     */
    [[nodiscard]] Resolved
    resolveDefaults() const
    {
        return resolveTunableDefaults(tunables_);
    }

    /**
     * @brief First stage: decide from a frame's header alone, before its payload is read.
     *
     * In order: a cluster-only type from outside the cluster, a missing required feature, a
     * compressed payload on a connection that did not negotiate compression, then either declared
     * size over @c "max_payload_bytes". The cost is the size ramp on the decoded size, charged on
     * admission and on refusal alike, except for a type older releases pass on as received, whose
     * refusals cost only @ref PenaltyClass::Trivial.
     *
     * @param frame What the header declares.
     * @param connection The connection the frame arrived on.
     * @param cfg The resolved tunables supplying the cap and the size ramp.
     * @return Admit carrying the size cost, or Drop with its reason and penalty class.
     */
    template <ConnectionView Connection>
    [[nodiscard]] AdmissionDecision
    admitHeader(FrameInfo const& frame, Connection const& connection, Resolved const& cfg) const
    {
        double const cost = costFor(cfg.template get<"size_ramp">(), frame.decodedBytes);
        auto const refuse = [&](std::string_view why, PenaltyClass penalty) {
            auto const applied = penaltyFor(penalty);
            return AdmissionDecision::drop(
                why, applied, applied == PenaltyClass::Trivial ? 0.0 : cost);
        };

        if (attributes_.fromClusterOnly and not connection.inCluster())
        {
            return refuse("only the cluster may send this type", PenaltyClass::Useless);
        }
        if (attributes_.requiresFeature and not connection.hasFeature(*attributes_.requiresFeature))
        {
            return refuse(
                "the connection did not negotiate this type's feature", PenaltyClass::Useless);
        }
        if (frame.compressed and not connection.compressionNegotiated())
        {
            return AdmissionDecision::drop(
                "compressed payload without negotiated compression", PenaltyClass::Malformed, cost);
        }
        auto const cap = cfg.template get<"max_payload_bytes">();
        if (frame.wireBytes > cap or frame.decodedBytes > cap)
        {
            return refuse("payload exceeds max bytes for this type", PenaltyClass::Useless);
        }
        return AdmissionDecision::admit(cost);
    }

    /**
     * @brief Make a fresh, state-carrying checker bound to @p cfg for one message walk.
     *
     * @param cfg The resolved tunables the check reads its thresholds from.
     * @return A callable taking a @ref VisitEvent and returning an @ref AdmissionDecision.
     */
    [[nodiscard]] auto
    makeChecker(Resolved const& cfg) const
    {
        if constexpr (std::same_as<Check, NoHook>)
        {
            return [](VisitEvent const&) { return AdmissionDecision::admit(); };
        }
        else
        {
            return [check = check_, &cfg](VisitEvent const& event) mutable {
                return check(event, cfg);
            };
        }
    }
};

/**
 * @brief Entry point for declaring an @ref AdmissionSpec for type @p T.
 *
 * Every spec must declare the well-known tunables @c "max_payload_bytes" and @c "size_ramp"; this
 * is enforced at compile time. Pass any additional named tunables the hooks need.
 *
 * @tparam T The type the spec is associated with (must be supplied explicitly).
 */
template <typename T, typename... Tunables>
[[nodiscard]] consteval auto
makeSpec(Tunables... tunables)
{
    using Resolved = ResolvedTunablesOfT<std::tuple<Tunables...>>;
    static_assert(
        Resolved::template has<"max_payload_bytes">(),
        R"(AdmissionSpec requires a tunable named "max_payload_bytes" (the hard drop cap).)");
    static_assert(
        Resolved::template has<"size_ramp">(),
        R"(AdmissionSpec requires a tunable named "size_ramp" (the size->cost ramp).)");
    return AdmissionSpec<T, std::tuple<Tunables...>>{
        std::tuple<Tunables...>{tunables...}, NoHook{}};
}

}  // namespace admission::spec
