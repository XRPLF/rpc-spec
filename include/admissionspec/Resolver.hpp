/** @file */
#pragma once

#include <admissionspec/detail/Resolve.hpp>

#include <cstddef>
#include <tuple>
#include <vector>

namespace admission::spec {

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
[[nodiscard]] Spec::Resolved
resolve(Spec const& spec, Config const& config)
{
    return detail::resolveTuple(spec.tunables(), config);
}

}  // namespace admission::spec
