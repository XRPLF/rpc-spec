#pragma once

#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/Types.hpp>

#include <cstddef>
#include <tuple>

namespace xrpl {
struct BasicConfig;
}

namespace admission::spec {

namespace detail {

/** @brief Resolve one scalar tunable: use the config override if the key is present, else the
 * default. */
template <FixedString Name, typename T>
[[nodiscard]] T
resolveOne(xrpl::BasicConfig const& config, Tunable<Name, T> const& t)
{
    // if (config.contains(t.configKey)) {
    //     if (auto const v = config.maybeValue<T>(t.configKey))
    //         return *v;
    // }
    return t.defaultValue;
}

/** @brief Resolve the ramp tunable: read an array of {up_to_bytes, cost} objects if present, else
 * the default.
 */
template <FixedString Name, std::size_t N>
[[nodiscard]] std::vector<SizeTier>
resolveOne(xrpl::BasicConfig const& config, Tunable<Name, SizeCostRamp<N>> const& t)
{
    std::vector<SizeTier> tiers;
    // if (config.contains(t.configKey)) {
    //     auto const arr = config.getArray(t.configKey);
    //     tiers.reserve(arr.size());
    //     for (std::size_t i = 0; i < arr.size(); ++i) {
    //         auto const obj = arr.objectAt(i);
    //         tiers.push_back(SizeTier{obj.template get<std::uint64_t>("up_to_bytes"), obj.template
    //         get<double>("cost")});
    //     }
    // }
    if (tiers.empty())
    {
        tiers.assign(std::begin(t.defaultValue.tiers), std::end(t.defaultValue.tiers));
    }
    return tiers;
}

template <typename... Tunables>
[[nodiscard]] ResolvedTunables<Tunables...>
resolveTuple(std::tuple<Tunables...> const& tunables, xrpl::BasicConfig const& config)
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
template <typename Spec>
[[nodiscard]] typename Spec::Resolved
resolve(Spec const& spec, xrpl::BasicConfig const& config)
{
    return detail::resolveTuple(spec.tunables(), config);
}

}  // namespace admission::spec
