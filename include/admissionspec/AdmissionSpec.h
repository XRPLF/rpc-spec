#pragma once

#include <admissionspec/Types.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <type_traits>

namespace admission::spec {

/**
 * @brief An author hook that inspects a raw serialized payload before deserialization.
 *
 * Receives the bytes and the resolved tunables (so thresholds are config-overridable). Runs on
 * bytes only — no structure yet — so it is limited to size/cheap-pattern checks. Returns a drop to
 * reject, or an admit whose @c tokenCost is added to the size-ramp cost.
 */
template <typename F, typename Resolved>
concept SomePreDeserializeHook =
    requires(F const f, std::span<std::byte const> bytes, Resolved const& cfg) {
        { f(bytes, cfg) } -> std::same_as<AdmissionDecision>;
    };

/**
 * @brief An author hook that inspects a fully deserialized value of type @p T.
 *
 * Receives the value and the resolved tunables — the place to encapsulate amplification invariants
 * over hydrated contents against config-tunable limits (e.g. "entries may not exceed max_entries").
 */
template <typename F, typename T, typename Resolved>
concept SomePostDeserializeHook = requires(F const f, T const& v, Resolved const& cfg) {
    { f(v, cfg) } -> std::same_as<AdmissionDecision>;
};

/// Sentinel for an unattached hook slot.
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
 *                tunable<"min_header_bytes">(std::size_t{4},
"admission.my_message.min_header_bytes"),
 *                tunable<"max_entries">(std::size_t{100}, "admission.my_message.max_entries")
 *            )
 *         // pre-deserialization: cheap byte-level reject before we pay to parse.
 *         .withPreCheck([](std::span<std::byte const> bytes, auto const& cfg) -> AdmissionDecision
{
 *             if (bytes.size() < cfg.template get<"min_header_bytes">())
 *                 // a drop may carry a message-specific penalty cost (see drop()'s second arg)
 *                 return AdmissionDecision::drop("payload too small to contain a header", 1.0);
 *             return AdmissionDecision::admit();  // size-ramp cost is added automatically
 *         })
 *         // post-deserialization: amplification invariant over the hydrated value.
 *         .withPostCheck([](MyMessage const& m, auto const& cfg) -> AdmissionDecision {
 *             if (m.entries.size() > cfg.template get<"max_entries">())
 *                 // penalize an amplification attempt harder than a benign reject
 *                 return AdmissionDecision::drop("too many entries", 4.0);
 *             return AdmissionDecision::admit();
 *         });
 * }
 * @endcode
 *
 * And at the ingress point, where a @ref ConnectionLimiter owns one token bucket per connection:
 *
 * @code
 * // Constructed once at startup from resolved config (see Resolver.hpp / BucketSettings).
 * ConnectionLimiter<ConnectionId> limiter{bucketSettings, maxConnections};
 *
 * void onFrame(ConnectionId conn, std::span<std::byte const> frame) {
 *     auto const now = std::chrono::steady_clock::now();
 *
 *     // 1. Pre-parse gate: byte cap + size-ramp cost + pre hook, debited from conn's bucket.
 *     if (limiter.admitPre<MyMessage>(conn, frame, now).dropped())
 *         return;  // dropped: oversize, malformed-by-size, or rate limited — never parsed.
 *
 *     // 2. Now it is safe to pay for deserialization.
 *     MyMessage msg = parse<MyMessage>(frame);
 *
 *     // 3. Post-parse gate: amplification invariant over the hydrated value.
 *     if (limiter.admitPost<MyMessage>(conn, msg, now).dropped())
 *         return;  // dropped: e.g. too many entries.
 *
 *     handle(msg);
 * }
 * @endcode
 *
 * @note Bucket capacity/refill are connection-scoped (see @ref BucketParams), not part of this
spec.
 */
template <typename T, typename TunablesTuple, typename PreHook = NoHook, typename PostHook = NoHook>
struct AdmissionSpec
{
public:
    using Type = T;
    using Resolved = ResolvedTunablesOfT<TunablesTuple>;

    consteval AdmissionSpec(TunablesTuple tunables, PreHook preHook, PostHook postHook)
        : tunables_{tunables}, preHook_{preHook}, postHook_{postHook}
    {
    }

    /** @brief Attach a pre-deserialization hook; returns the updated spec. */
    template <SomePreDeserializeHook<Resolved> H>
    [[nodiscard]] consteval auto
    withPreCheck(H hook) const
    {
        return AdmissionSpec<T, TunablesTuple, H, PostHook>{tunables_, hook, postHook_};
    }

    /** @brief Attach a post-deserialization hook; returns the updated spec. */
    template <typename H>
        requires SomePostDeserializeHook<H, T, Resolved>
    [[nodiscard]] consteval auto
    withPostCheck(H hook) const
    {
        return AdmissionSpec<T, TunablesTuple, PreHook, H>{tunables_, preHook_, hook};
    }

    [[nodiscard]] constexpr TunablesTuple const&
    tunables() const noexcept
    {
        return tunables_;
    }

    /**
     * @brief Build a @ref ResolvedTunables from the spec's defaults (no config overrides).
     *
     * The T3 config resolver will provide the analogous `resolve(config)` producing the same
     * Resolved type with overridden values; until then this keeps the stages usable.
     */
    [[nodiscard]] Resolved
    resolveDefaults() const
    {
        return resolveTunableDefaults(tunables_);
    }

    /**
     * @brief Pre-deserialization stage: enforce the hard byte cap, compute the size cost, run the
     * pre hook.
     */
    [[nodiscard]] AdmissionDecision
    preAdmit(std::span<std::byte const> payload, Resolved const& cfg) const
    {
        // Cost the payload would incur if admitted. For an oversize payload this is the top ramp
        // tier (costFor saturates at the last tier), which we reuse as the drop penalty so the
        // cheapest reject to generate is not free to spam.
        double cost =
            costFor(cfg.template get<"size_ramp">(), static_cast<std::uint64_t>(payload.size()));

        if (payload.size() > cfg.template get<"max_payload_bytes">())
        {
            return AdmissionDecision::drop("payload exceeds max bytes for this type", cost);
        }

        if constexpr (!std::same_as<PreHook, NoHook>)
        {
            auto const decision = preHook_(payload, cfg);
            if (decision.dropped())
            {
                return decision;
            }
            cost += decision.tokenCost;
        }
        return AdmissionDecision::admit(cost);
    }

    /**
     * @brief Post-deserialization stage: run the post hook against the hydrated value.
     */
    [[nodiscard]] AdmissionDecision
    postAdmit([[maybe_unused]] T const& value, [[maybe_unused]] Resolved const& cfg) const
    {
        if constexpr (std::same_as<PostHook, NoHook>)
        {
            return AdmissionDecision::admit();
        }
        else
        {
            return postHook_(value, cfg);
        }
    }

private:
    TunablesTuple tunables_;
    PreHook preHook_;
    PostHook postHook_;
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
        std::tuple<Tunables...>{tunables...}, NoHook{}, NoHook{}};
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

/// True when @p T has an associated admission spec (via ADL overload or trait specialization).
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
 * @brief The resolved tunables for type @p T.
 *
 * Currently built from spec defaults and cached in a function-local static. When the config
 * resolver (T3) lands, this is where the config-overridden values will be sourced.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] auto const&
resolvedFor()
{
    static auto const kRESOLVED = admissionSpecFor<T>().resolveDefaults();
    return kRESOLVED;
}

/** @brief Run the pre-deserialization stage for type @p T against a raw payload. */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] AdmissionDecision
preAdmit(std::span<std::byte const> payload)
{
    static constexpr auto kSPEC = admissionSpecFor<T>();
    return kSPEC.preAdmit(payload, resolvedFor<T>());
}

/** @brief Run the post-deserialization stage for type @p T against a hydrated value. */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] AdmissionDecision
postAdmit(T const& value)
{
    static constexpr auto kSPEC = admissionSpecFor<T>();
    return kSPEC.postAdmit(value, resolvedFor<T>());
}

}  // namespace admission::spec
