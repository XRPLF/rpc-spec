/** @file */
#pragma once

#include <admissionspec/FixedString.hpp>
#include <admissionspec/ResolvedTunables.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/Tunable.hpp>

#include <cstddef>
#include <tuple>
#include <vector>

namespace admission::spec::detail {

/**
 * @brief Resolve one scalar tunable: use the config override if the key is present, else the
 * default.
 */
template <typename Config, FixedString Name, typename T>
[[nodiscard]] T
resolveOne(Config const& config, Tunable<Name, T> const& tunable)
{
    if (auto const value = config.template maybeValue<T>(tunable.configKey))
    {
        return *value;
    }
    return tunable.defaultValue;
}

/**
 * @brief Resolve the ramp tunable: read an array of {up_to_bytes, cost} objects if present, else
 * the default.
 */
template <typename Config, FixedString Name, size_t N>
[[nodiscard]] std::vector<admission::spec::SizeTier>
resolveOne(Config const& config, Tunable<Name, SizeCostRamp<N>> const& tunable)
{
    std::vector<admission::spec::SizeTier> tiers;
    if (auto const arr =
            config.template maybeValue<std::vector<std::pair<uint64_t, double>>>(tunable.configKey))
    {
        tiers.reserve(arr->size());
        for (auto const& obj : *arr)
        {
            tiers.push_back(admission::spec::SizeTier{obj.first, obj.second});
        }
    }
    if (tiers.empty())
    {
        tiers.assign(std::begin(tunable.defaultValue.tiers), std::end(tunable.defaultValue.tiers));
    }
    return tiers;
}

template <typename Config, typename... Tunables>
[[nodiscard]] ResolvedTunables<Tunables...>
resolveTuple(std::tuple<Tunables...> const& tunables, Config const& config)
{
    return std::apply(
        [&](Tunables const&... tunable) {
            return ResolvedTunables<Tunables...>{resolveOne(config, tunable)...};
        },
        tunables);
}

}  // namespace admission::spec::detail
