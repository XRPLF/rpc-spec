#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace admission::spec {

/**
 * @brief Outcome of an admission stage: admit (with a token cost) or drop (with a reason).
 */
enum class AdmissionAction { Admit, Drop };

/**
 * @brief The result an admission stage produces.
 *
 * @c tokenCost is the number of tokens the caller should debit from the connection's bucket. On an
 * admit it is the processing cost of the message; on a drop it is a penalty debited even though the
 * message is rejected, so cheap-to-generate rejects (oversize, malformed) still apply backpressure
 * instead of being free to spam. @c reason carries a human-readable explanation for a drop and is
 * empty when admitted. Stages never debit a bucket themselves — they only compute the cost — so
 * bucket exhaustion is a separate drop applied at the call site.
 */
struct AdmissionDecision
{
    AdmissionAction action{AdmissionAction::Admit};
    double tokenCost{0.0};
    std::string_view reason;

    [[nodiscard]] constexpr bool
    admitted() const noexcept
    {
        return action == AdmissionAction::Admit;
    }

    [[nodiscard]] constexpr bool
    dropped() const noexcept
    {
        return action == AdmissionAction::Drop;
    }

    /** @brief Construct an "admit" decision with the given token cost. */
    [[nodiscard]] static constexpr AdmissionDecision
    admit(double cost = 0.0) noexcept
    {
        return {.action = AdmissionAction::Admit, .tokenCost = cost, .reason = {}};
    }

    /**
     * @brief Construct a "drop" decision, optionally charging a token penalty.
     *
     * @p cost is debited from the connection's bucket as a penalty even though the message is
     * rejected, so cheap-to-generate rejects (oversize, malformed) still apply backpressure instead
     * of being free to spam. Defaults to 0 for callers that do not penalize.
     */
    [[nodiscard]] static constexpr AdmissionDecision
    drop(std::string_view why, double cost = 0.0) noexcept
    {
        return {.action = AdmissionAction::Drop, .tokenCost = cost, .reason = why};
    }

    bool
    operator<=>(AdmissionDecision const&) const = default;
};

/**
 * @brief A compile-time string usable as a non-type template parameter (for naming tunables).
 */
template <std::size_t N>
struct FixedString
{
    char value[N]{};

    consteval FixedString(char const (&str)[N]) noexcept
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            value[i] = str[i];
        }
    }

    [[nodiscard]] constexpr std::string_view
    view() const noexcept
    {
        return std::string_view{value, N - 1};
    }

    bool
    operator<=>(FixedString const&) const = default;
};

template <std::size_t N>
FixedString(char const (&)[N]) -> FixedString<N>;

/**
 * @brief One step of a size→cost ramp: payloads up to @c upToBytes cost @c cost tokens.
 */
struct SizeTier
{
    std::uint64_t upToBytes{};  ///< inclusive upper bound, in bytes, for this tier
    double cost{};              ///< tokens charged for a payload whose size falls in this tier

    bool
    operator<=>(SizeTier const&) const = default;
};

/**
 * @brief A monotonically increasing ramp mapping serialized payload size to a token cost.
 *
 * Tiers are expected ascending by @c upToBytes. This is the *compile-time default* representation;
 * when resolved (see @ref ResolvedTypeOf) it becomes a @c std::vector<SizeTier> so config can
 * replace the whole ramp, tier count and all.
 */
template <std::size_t N>
struct SizeCostRamp
{
    std::array<SizeTier, N> tiers{};

    SizeCostRamp() = default;

    consteval SizeCostRamp(std::array<SizeTier, N> t) noexcept : tiers{t}
    {
    }

    bool
    operator<=>(SizeCostRamp const&) const = default;
};

template <std::size_t N>
SizeCostRamp(std::array<SizeTier, N>) -> SizeCostRamp<N>;

/**
 * @brief Build a @ref SizeCostRamp from a braced list of tiers, e.g.
 *        @code ramp({{1024, 0.5}, {5 * 1024, 1.0}, {64 * 1024, 4.0}}) @endcode
 */
template <std::size_t N>
[[nodiscard]] consteval SizeCostRamp<N>
ramp(SizeTier const (&tiers)[N])
{
    std::array<SizeTier, N> arr{};
    for (std::size_t i = 0; i < N; ++i)
    {
        arr[i] = tiers[i];
    }
    return SizeCostRamp<N>{arr};
}

/** @brief Token cost for a payload of @p bytes against a (resolved) list of tiers. */
[[nodiscard]] constexpr double
costFor(std::span<SizeTier const> tiers, std::uint64_t bytes) noexcept
{
    for (auto const& tier : tiers)
    {
        if (bytes <= tier.upToBytes)
        {
            return tier.cost;
        }
    }
    return tiers.empty() ? 0.0 : tiers.back().cost;
}

/**
 * @brief A named, config-overridable configuration value — the single currency of an @ref
 * AdmissionSpec.
 *
 * Every configurable value (the hard drop cap, the size→cost ramp, and any hook thresholds) is a
 * @c Tunable: a compile-time default plus the config-file key that overrides it, tagged with a
 * compile-time @p Name so it can be looked up type-safely after resolution.
 *
 * @tparam Name Compile-time identifier (e.g. "max_payload_bytes", "size_ramp", "max_entries").
 * @tparam T Default value type.
 */
template <FixedString Name, typename T>
struct Tunable
{
    using ValueType = T;
    static constexpr std::string_view kName = Name.view();

    T defaultValue{};
    std::string_view configKey;

    bool
    operator<=>(Tunable const&) const = default;
};

/**
 * @brief Build a named @ref Tunable, e.g.
 *        @code tunable<"max_entries">(std::size_t{100}, "admission.x.max_entries") @endcode
 */
template <FixedString Name, typename T>
[[nodiscard]] consteval Tunable<Name, T>
tunable(T defaultValue, std::string_view configKey) noexcept
{
    return Tunable<Name, T>{defaultValue, configKey};
}

/**
 * @brief Maps a tunable's compile-time default type to its resolved runtime type.
 *
 * Identity for scalars; a @ref SizeCostRamp resolves to a @c std::vector<SizeTier> (config may
 * change the tier count). Specialize this for any other default type whose resolved form differs.
 */
template <typename T>
struct ResolvedTypeOf
{
    using type = T;
};

template <std::size_t N>
struct ResolvedTypeOf<SizeCostRamp<N>>
{
    using type = std::vector<SizeTier>;
};

template <typename T>
using ResolvedTypeOfT = typename ResolvedTypeOf<T>::type;

/** @brief Convert a tunable default value to its resolved runtime representation. */
template <typename T>
[[nodiscard]] constexpr T
toResolved(T const& value)
{
    return value;
}

template <std::size_t N>
[[nodiscard]] inline std::vector<SizeTier>
toResolved(SizeCostRamp<N> const& r)
{
    return std::vector<SizeTier>(std::begin(r.tiers), std::end(r.tiers));
}

namespace detail {

template <FixedString Name, typename... Tunables>
[[nodiscard]] consteval std::size_t
tunableIndex() noexcept
{
    std::size_t idx = sizeof...(Tunables);
    std::size_t i = 0;
    ((Tunables::kName == Name.view() ? static_cast<void>(idx = i) : static_cast<void>(0), ++i),
     ...);
    return idx;
}

}  // namespace detail

/**
 * @brief The resolved values of a spec's tunables, looked up by compile-time name.
 *
 * Produced by resolving each @ref Tunable against the config (or its default). Both the eval stages
 * and the author hooks query it the same way: @code resolved.get<"size_ramp">() @endcode.
 *
 * @tparam Tunables The @ref Tunable types the owning spec declared.
 */
template <typename... Tunables>
class ResolvedTunables
{
public:
    constexpr explicit ResolvedTunables(ResolvedTypeOfT<typename Tunables::ValueType>... vals)
        : values_{std::move(vals)...}
    {
    }

    /** @brief Resolved value of the tunable named @p Name. */
    template <FixedString Name>
    [[nodiscard]] constexpr auto const&
    get() const noexcept
    {
        constexpr std::size_t kIDX = detail::tunableIndex<Name, Tunables...>();
        static_assert(kIDX < sizeof...(Tunables), "ResolvedTunables::get: unknown tunable name");
        return std::get<kIDX>(values_);
    }

    /** @brief Whether a tunable named @p Name was declared. */
    template <FixedString Name>
    [[nodiscard]] static constexpr bool
    has() noexcept
    {
        return detail::tunableIndex<Name, Tunables...>() < sizeof...(Tunables);
    }

private:
    std::tuple<ResolvedTypeOfT<typename Tunables::ValueType>...> values_;
};

/** @brief Maps a `std::tuple<Tunable...>` to the corresponding @ref ResolvedTunables type. */
template <typename TunablesTuple>
struct ResolvedTunablesOf;

template <typename... Tunables>
struct ResolvedTunablesOf<std::tuple<Tunables...>>
{
    using type = ResolvedTunables<Tunables...>;
};

template <typename TunablesTuple>
using ResolvedTunablesOfT = typename ResolvedTunablesOf<TunablesTuple>::type;

/** @brief Build a @ref ResolvedTunables from a tunable tuple using each tunable's default value. */
template <typename... Tunables>
[[nodiscard]] inline ResolvedTunables<Tunables...>
resolveTunableDefaults(std::tuple<Tunables...> const& tunables)
{
    return std::apply(
        [](Tunables const&... t) {
            return ResolvedTunables<Tunables...>{toResolved(t.defaultValue)...};
        },
        tunables);
}

/**
 * @brief Connection-scoped token-bucket parameters.
 *
 * There is exactly ONE bucket per connection, and a message type only affects the *cost* charged
 * against it — types never get their own bucket. These parameters therefore live at connection
 * scope (resolved once from config), and are intentionally NOT part of @ref AdmissionSpec.
 */
struct BucketParams
{
    Tunable<"capacity", double> capacity;
    Tunable<"refill_rate_per_second", double> refillRatePerSecond;
};

/**
 * @brief The resolved (config-overridden) connection-bucket parameters the limiter is constructed
 * with.
 */
struct BucketSettings
{
    double capacity{};
    double refillRatePerSecond{};
};

}  // namespace admission::spec
