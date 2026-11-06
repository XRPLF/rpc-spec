/** @file */
#pragma once

#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/Frame.hpp>

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace admission::spec {

/**
 * @brief The outcome of the header stage: the decision, and where the sender stands after its
 * charge.
 */
struct HeaderAdmission
{
    AdmissionDecision decision;
    Standing standing{Standing::Ok};
};

/**
 * @brief Run the header stage for type @p T and charge its cost.
 *
 * A sender already at @ref Standing::Drop is refused unread and not charged again. Otherwise the
 * spec decides from the header, and the decision's cost and penalty are charged, admitted or not.
 * The host acts on the result: it skips a refused frame's bytes, and stops serving a sender whose
 * standing is @ref Standing::Drop.
 *
 * @param frame What the frame's header declares.
 * @param connection The connection the frame arrived on.
 * @param sink Where to charge.
 */
template <typename T, ConnectionView Connection, ChargeSink Sink>
    requires HasAdmissionSpec<T>
[[nodiscard]] HeaderAdmission
admitHeader(FrameInfo const& frame, Connection const& connection, Sink& sink)
{
    if (sink.standing() == Standing::Drop)
    {
        return {
            .decision = AdmissionDecision::drop("sender is over its limit", PenaltyClass::None),
            .standing = Standing::Drop};
    }

    static constexpr auto kSpec = admissionSpecFor<T>();
    auto const decision = kSpec.admitHeader(frame, connection, resolvedFor<T>());
    return {.decision = decision, .standing = sink.charge(chargeFor(decision))};
}

/**
 * @brief Charge a refusal found after the header stage, with the penalty @p T's spec allows.
 *
 * The header stage already charged the message's size, so this charges only the penalty.
 *
 * @param decision The later stage's drop.
 * @param sink Where to charge.
 * @return Where the sender stands after the charge.
 */
template <typename T, ChargeSink Sink>
    requires HasAdmissionSpec<T>
Standing
chargeRefusal(AdmissionDecision const& decision, Sink& sink)
{
    static constexpr auto kSpec = admissionSpecFor<T>();
    return sink.charge(
        {.cost = 0.0, .penalty = kSpec.penaltyFor(decision.penalty), .reason = decision.reason});
}

/**
 * @brief The type number @p T's spec declares.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] consteval uint32_t
typeIdOf()
{
    constexpr auto kTypeId = admissionSpecFor<T>().attributes().typeId;
    static_assert(kTypeId.has_value(), "SpecRegistry: a spec in a registry must declare a typeId");
    return *kTypeId;
}

/**
 * @brief Maps a frame's type number to the type whose spec governs it.
 *
 * @code
 * using PeerSpecs = SpecRegistry<TMPing, TMManifests, TMGetLedger>;
 * auto const result = PeerSpecs::dispatch(
 *     frame.type,
 *     [&]<class T> { return admitHeader<T>(frame, connection, sink); },
 *     [&] { return HeaderAdmission{AdmissionDecision::drop("unknown type", PenaltyClass::Useless)};
 * });
 * @endcode
 */
template <typename... Ts>
    requires(HasAdmissionSpec<Ts> and ...)
struct SpecRegistry
{
    /**
     * @brief Whether every type number in the registry is distinct.
     */
    [[nodiscard]] static consteval bool
    distinctIds()
    {
        constexpr auto kIds = std::array<uint32_t, sizeof...(Ts)>{typeIdOf<Ts>()...};
        for (size_t i = 0; i < kIds.size(); ++i)
        {
            for (size_t j = i + 1; j < kIds.size(); ++j)
            {
                if (kIds[i] == kIds[j])
                {
                    return false;
                }
            }
        }
        return true;
    }

    static_assert(distinctIds(), "SpecRegistry: two specs declare the same typeId");

    /**
     * @brief Call @p known with the type @p type names, or @p unknown if it names none.
     *
     * @param type The type number from the frame header.
     * @param known Called as `known.template operator()<T>()`.
     * @param unknown Called with no arguments. Its result type is the result type of every call.
     * @return What the call returns.
     */
    template <typename Known, typename Unknown>
    static auto
    dispatch(uint32_t type, Known&& known, Unknown&& unknown) -> std::invoke_result_t<Unknown&>
    {
        using Result = std::invoke_result_t<Unknown&>;
        if constexpr (std::is_void_v<Result>)
        {
            bool const found =
                ((type == typeIdOf<Ts>() ? (known.template operator()<Ts>(), true) : false) or ...);
            if (not found)
            {
                unknown();
            }
        }
        else
        {
            Result result{};
            bool const found =
                ((type == typeIdOf<Ts>() ? (result = known.template operator()<Ts>(), true)
                                         : false) or
                 ...);
            if (not found)
            {
                return unknown();
            }
            return result;
        }
    }
};

}  // namespace admission::spec
