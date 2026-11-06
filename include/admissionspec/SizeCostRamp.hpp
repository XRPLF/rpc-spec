/** @file */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace admission::spec {

/**
 * @brief One step of a size→cost ramp: payloads up to @c upToBytes cost @c cost tokens.
 */
struct SizeTier
{
    /**
     * @brief Inclusive upper bound, in bytes, for this tier.
     */
    uint64_t upToBytes{};

    /**
     * @brief Tokens charged for a payload whose size falls in this tier.
     */
    double cost{};

    /**
     * @brief Compare two values of this type.
     *
     * @return The comparison result.
     */
    bool
    operator==(SizeTier const&) const = default;
};

/**
 * @brief A monotonically increasing ramp mapping serialized payload size to a token cost.
 *
 * Tiers are expected ascending by @c upToBytes. This is the *compile-time default* representation;
 * when resolved (see @ref ResolvedTypeOf) it becomes a @c std::vector<SizeTier> so config can
 * replace the whole ramp, tier count and all.
 */
template <size_t N>
struct SizeCostRamp
{
    /**
     * @brief Size tiers, in ascending order of @ref SizeTier::upToBytes.
     */
    std::array<SizeTier, N> tiers{};

    SizeCostRamp() = default;

    /**
     * @brief Construct a @ref SizeCostRamp.
     *
     * @param tiers The size tiers, in ascending order of upper bound.
     */
    consteval SizeCostRamp(std::array<SizeTier, N> tiers) noexcept : tiers{tiers}
    {
    }

    /**
     * @brief Compare two values of this type.
     *
     * @return The comparison result.
     */
    bool
    operator==(SizeCostRamp const&) const = default;
};

template <size_t N>
SizeCostRamp(std::array<SizeTier, N>) -> SizeCostRamp<N>;

/**
 * @brief Build a @ref SizeCostRamp from a braced list of tiers, e.g.
 *        @code ramp({{1024, 0.5}, {5 * 1024, 1.0}, {64 * 1024, 4.0}}) @endcode
 */
template <size_t N>
[[nodiscard]] consteval SizeCostRamp<N>
ramp(SizeTier const (&tiers)[N])
{
    std::array<SizeTier, N> arr{};
    for (auto i = 0uz; i < N; ++i)
    {
        arr[i] = tiers[i];
    }
    return SizeCostRamp<N>{arr};
}

/**
 * @brief Token cost for a payload of @p bytes against a (resolved) list of tiers.
 */
[[nodiscard]] constexpr double
costFor(std::span<SizeTier const> tiers, uint64_t bytes) noexcept
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

}  // namespace admission::spec
