#pragma once

#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/Types.hpp>

#include <cstddef>
#include <tuple>
#include <vector>

namespace admission::spec {

namespace detail {

/** @brief Resolve one scalar tunable: use the config override if the key is present, else the
 * default. */
template <typename Config, FixedString Name, typename T>
[[nodiscard]] T
resolveOne(Config const& config, Tunable<Name, T> const& t)
{
    if (auto const v = config.template maybeValue<T>(t.configKey))
    {
        return *v;
    }
    return t.defaultValue;
}

/** @brief Resolve the ramp tunable: read an array of {up_to_bytes, cost} objects if present, else
 * the default.
 */
template <typename Config, FixedString Name, size_t N>
[[nodiscard]] std::vector<admission::spec::SizeTier>
resolveOne(Config const& config, Tunable<Name, SizeCostRamp<N>> const& t)
{
    std::vector<admission::spec::SizeTier> tiers;
    if (auto const arr =
            config.template maybeValue<std::vector<std::pair<uint64_t, double>>>(t.configKey))
    {
        tiers.reserve(arr->size());
        for (auto const& obj : *arr)
        {
            tiers.push_back(admission::spec::SizeTier{obj.first, obj.second});
        }
    }
    if (tiers.empty())
    {
        tiers.assign(std::begin(t.defaultValue.tiers), std::end(t.defaultValue.tiers));
    }
    return tiers;
}

template <typename Config, typename... Tunables>
[[nodiscard]] ResolvedTunables<Tunables...>
resolveTuple(std::tuple<Tunables...> const& tunables, Config const& config)
{
    return std::apply(
        [&](Tunables const&... t) {
            return ResolvedTunables<Tunables...>{resolveOne(config, t)...};
        },
        tunables);
}

}  // namespace detail

/**
 * @brief Resolve a spec's tunables against the runtime config, producing the bag the stages
 * evaluate against.
 *
 * For each @ref Tunable, the override at its @c configKey is used when present, otherwise the
 * compile-time default. Scalars come from @c maybeValue; the size ramp is read from an array of @c
 * {up_to_bytes, cost} objects.
 *
 * @note clio's config is a closed schema: each @c configKey (and the ramp's array sub-keys) must be
 *       registered in the config definition for the override to be readable. Keys absent from the
 * schema simply fall back to the spec default. See @ref BucketParams for the connection-scoped
 * bucket settings, which are resolved separately and handed to @ref ConnectionLimiter.
 */
template <typename Spec, typename Config>
[[nodiscard]] typename Spec::Resolved
resolve(Spec const& spec, Config const& config)
{
    return detail::resolveTuple(spec.tunables(), config);
}

}  // namespace admission::spec
