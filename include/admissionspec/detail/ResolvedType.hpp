/** @file */
#pragma once

#include <admissionspec/SizeCostRamp.hpp>

#include <cstddef>
#include <vector>

namespace admission::spec::detail {

/**
 * @brief Maps a tunable's compile-time default type to its resolved runtime type.
 *
 * Identity for scalars; a @ref SizeCostRamp resolves to a @c std::vector<SizeTier> (config may
 * change the tier count). Specialize this for any other default type whose resolved form differs.
 */
template <typename T>
struct ResolvedTypeOf
{
    /**
     * @brief The resolved type.
     */
    using type = T;
};

/**
 * @brief Maps a tunable's declared type to the type it resolves to.
 */
template <size_t N>
struct ResolvedTypeOf<SizeCostRamp<N>>
{
    /**
     * @brief The resolved type.
     */
    using type = std::vector<SizeTier>;
};

template <typename T>
using ResolvedTypeOfT = ResolvedTypeOf<T>::type;

/**
 * @brief Convert a tunable default value to its resolved runtime representation.
 */
template <typename T>
[[nodiscard]] constexpr T
toResolved(T const& value)
{
    return value;
}

template <size_t N>
[[nodiscard]] inline std::vector<SizeTier>
toResolved(SizeCostRamp<N> const& ramp)
{
    return std::vector<SizeTier>(std::begin(ramp.tiers), std::end(ramp.tiers));
}

}  // namespace admission::spec::detail
