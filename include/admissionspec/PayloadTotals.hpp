/** @file */
#pragma once

#include <cstdint>

namespace admission::spec {

/**
 * @brief What a walk counted in a whole payload, so a host can observe it before it limits it.
 *
 * Counted up to where the walk stopped: all of the payload if it was admitted, up to the refusal
 * otherwise. A walker that does not count reports zeros.
 */
struct PayloadTotals
{
    /**
     * @brief The fields the schema does not define, in every message of the payload, counting an
     * undefined value of a closed enum as one.
     */
    uint64_t unknownFields{};

    /**
     * @brief The bytes in those fields, as @ref BaseLimits::maxUnknownFieldBytes counts them.
     */
    uint64_t unknownFieldBytes{};

    constexpr bool
    operator==(PayloadTotals const&) const = default;
};

}  // namespace admission::spec
