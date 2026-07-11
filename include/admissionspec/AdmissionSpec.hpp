#pragma once

#include <admissionspec/Types.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <type_traits>

namespace admission::spec {

/**
 * @brief A streaming check invoked by a caller-provided visitor as it emits @ref VisitEvent%s.
 */
template <typename F, typename Resolved>
concept SomeCheck = requires(F f, VisitEvent const& e, Resolved const& cfg) {
    { f(e, cfg) } -> std::same_as<AdmissionDecision>;
};

/**  Sentinel for an unattached hook slot. */
struct NoHook
{
};

/**
 * @brief Declarative admission-control policy associated with a C++ type @p T.
 *
 * A spec is just a bag of named @ref Tunable values plus optional author hooks. Two tunables are
 * required by every spec — @c "max_payload_bytes" (the hard drop cap) and @c "size_ramp" (the
size→cost
 * ramp) — and @ref makeSpec @c static_asserts their presence. Everything else is an extra tunable
the
 * hooks read by name. Hooks receive the resolved bag, so their thresholds are config-overridable
too.
 *
 * The type is consteval-constructed and meant to live as a @c static @c constexpr object;
evaluation
 * runs against a @ref ResolvedTunables produced by resolving the bag.
 *
 * @code
 * consteval auto admissionSpec(std::type_identity<MyMessage>) {
 *     using namespace util::admission;
 *     return makeSpec<MyMessage>(
 *                tunable<"max_payload_bytes">(std::uint64_t{64 * 1024},
"admission.my_message.max_payload_bytes"),
 *                tunable<"size_ramp">(ramp({{1024, 0.5}, {64 * 1024, 4.0}}),
"admission.my_message.size_ramp"),
 *                tunable<"max_entries">(std::size_t{100}, "admission.my_message.max_entries")
 *            )
 *         // streaming: the check sees one event at a time and keeps whatever state it needs, so an
 *         // amplification attempt is rejected before the message is ever fully hydrated.
 *         .withCheck(EntriesCap{});   // a small stateful functor, e.g.:
 * }
 *
 * // Counts elements of the `entries` list (field 3) and fails on the (max_entries+1)th — no
 * // buffering, no waiting for the list to close. A fresh EntriesCap runs per message.
 * struct EntriesCap {
 *     bool inEntries = false;
 *     std::size_t count = 0;
 *     AdmissionDecision operator()(VisitEvent const& e, auto const& cfg) {
 *         if (e.kind == EventKind::BeginArray && e.fieldNumber == 3)
 *         {
 *           inEntries = true;
 *           count = 0;
 *         }
 *         else if (e.kind == EventKind::EndArray && e.fieldNumber == 3)
 *         {
 *           inEntries = false;
 *         }
 *         else if (inEntries && e.kind == EventKind::Scalar && ++count > cfg.template
get<"max_entries">())
 *         {
 *             // penalize an amplification attempt harder than a benign reject
 *             return AdmissionDecision::drop("too many entries", 4.0);
 *         }
 *         return AdmissionDecision::admit();
 *     }
 * };
 * @endcode
 *
 * And at the ingress point, where a @ref ConnectionLimiter owns one token bucket per connection.
The
 * caller owns the traversal — it hands the limiter a visitor that decodes the raw payload (SAX for
 * JSON, a tag-walk for protobuf) and invokes the per-attribute check on each node it decodes:
 *
 * @code
 * // Constructed once at startup from resolved config (see Resolver.hpp / BucketSettings).
 * auto limiter = ConnectionLimiter<ConnectionId>{bucketSettings, maxConnections};
 *
 * void onFrame(ConnectionId conn, std::span<std::byte const> frame) {
 *     auto const now = std::chrono::steady_clock::now();
 *
 *     // 1. Pre-parse gate: hard byte cap + size-ramp cost, debited from conn's bucket.
 *     if (limiter.admitPre<MyMessage>(conn, frame, now).dropped())
 *     {
 *         return;  // dropped: oversize or rate limited — never parsed.
 *     }
 *
 *     // 2. Streaming gate: our visitor drives the decode and calls checkAttr per attribute,
 *     //    short-circuiting on the first drop — the message is never fully hydrated on reject.
 *     auto walk = [frame](auto checkAttr) -> AdmissionDecision {
 *         return walkMyMessage(frame, checkAttr);  // caller-provided traversal
 *     };
 *     if (limiter.admit<MyMessage>(conn, walk, now).dropped())
 *     {
 *         return;  // dropped: e.g. too many entries.
 *     }
 *
 *     handle(parse<MyMessage>(frame));
 * }
 * @endcode
 *
 * @note Bucket capacity/refill are connection-scoped (see @ref BucketParams), not part of this
spec.
 */
template <typename T, typename TunablesTuple, typename Check = NoHook>
class AdmissionSpec
{
public:
    using Type = T;
    using Resolved = ResolvedTunablesOfT<TunablesTuple>;

    consteval AdmissionSpec(TunablesTuple tunables, Check check)
        : tunables_{std::move(tunables)}, check_{std::move(check)}
    {
    }

    /**
     * @brief Attach a streaming check; returns the updated spec.
     */
    template <typename C>
        requires SomeCheck<C, Resolved>
    [[nodiscard]] consteval auto
    withCheck(C check) const
    {
        return AdmissionSpec<T, TunablesTuple, C>{tunables_, check};
    }

    [[nodiscard]] constexpr TunablesTuple const&
    tunables() const noexcept
    {
        return tunables_;
    }

    /**
     * @brief Build a @ref ResolvedTunables from the spec's defaults (no config overrides).
     */
    [[nodiscard]] Resolved
    resolveDefaults() const
    {
        return resolveTunableDefaults(tunables_);
    }

    /**
     * @brief Pre-deserialization stage: enforce the hard byte cap and compute the size cost.
     */
    [[nodiscard]] AdmissionDecision
    preAdmit(std::span<std::byte const> payload, Resolved const& cfg) const
    {
        double cost =
            costFor(cfg.template get<"size_ramp">(), static_cast<std::uint64_t>(payload.size()));

        if (payload.size() > cfg.template get<"max_payload_bytes">())
        {
            return AdmissionDecision::drop("payload exceeds max bytes for this type", cost);
        }

        return AdmissionDecision::admit(cost);
    }

    /**
     * @brief Make a fresh, state-carrying checker bound to @p cfg for one message walk.
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
            return [check = check_, &cfg](VisitEvent const& e) mutable { return check(e, cfg); };
        }
    }

private:
    TunablesTuple tunables_;
    Check check_;
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

/**
 * @brief Trait escape hatch for associating a spec with a foreign type @p T.
 *
 * For types you own, prefer an ADL-visible `consteval auto admissionSpec(std::type_identity<T>)`
 * overload in @p T's namespace. For foreign types, specialize this trait with a `static consteval
 * auto spec()`.
 */
template <typename T>
struct AdmissionTraits;  // primary template intentionally undefined

namespace detail {

template <typename T>
concept HasAdmissionAdl = requires { admissionSpec(std::type_identity<T>{}); };

template <typename T>
concept HasAdmissionTrait = requires { AdmissionTraits<T>::spec(); };

}  // namespace detail

/**
 * @brief True when @p T has an associated admission spec (via ADL overload or trait
 * specialization).
 */
template <typename T>
concept HasAdmissionSpec = detail::HasAdmissionAdl<T> || detail::HasAdmissionTrait<T>;

/**
 * @brief Resolve the @ref AdmissionSpec associated with @p T.
 */
template <typename T>
[[nodiscard]] consteval auto
admissionSpecFor()
{
    if constexpr (detail::HasAdmissionAdl<T>)
    {
        return admissionSpec(std::type_identity<T>{});
    }
    else
    {
        static_assert(
            detail::HasAdmissionTrait<T>,
            "No AdmissionSpec for T: provide an ADL 'admissionSpec(std::type_identity<T>)' "
            "overload in "
            "T's namespace, or specialize util::admission::AdmissionTraits<T>.");
        return AdmissionTraits<T>::spec();
    }
}

/**
 * @brief The (otherwise unnameable) @ref AdmissionSpec type associated with @p T.
 */
template <typename T>
    requires HasAdmissionSpec<T>
using SpecOf = decltype(admissionSpecFor<T>());

/**
 * @brief The resolved-tunables type of @p T's spec (what a check's @c cfg argument is).
 */
template <typename T>
    requires HasAdmissionSpec<T>
using ResolvedOf = typename SpecOf<T>::Resolved;

/**
 * @brief The resolved tunables for type @p T.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] auto const&
resolvedFor()
{
    static auto const kResolved = admissionSpecFor<T>().resolveDefaults();
    return kResolved;
}

/**
 * @brief Run the pre-deserialization stage for type @p T against a raw payload.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] AdmissionDecision
preAdmit(std::span<std::byte const> payload)
{
    static constexpr auto kSpec = admissionSpecFor<T>();
    return kSpec.preAdmit(payload, resolvedFor<T>());
}

/**
 * @brief Make a fresh streaming checker for type @p T, bound to its resolved tunables.
 *
 * The binding point between a caller-provided payload visitor and the type's @ref AdmissionSpec:
 * call once per message to get an `AdmissionDecision(VisitEvent const&)` callable, then feed it
 * each event and stop on the first drop. See @ref ConnectionLimiter::admit.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] auto
makeChecker()
{
    static constexpr auto kSpec = admissionSpecFor<T>();
    return kSpec.makeChecker(resolvedFor<T>());
}

}  // namespace admission::spec
