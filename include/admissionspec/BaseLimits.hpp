/** @file */
#pragma once

#include <cstdint>

namespace admission::spec {

/**
 * @brief Limits applied to every message while it is walked, unless its spec's own check refuses
 * sooner. The host may override the defaults from its configuration.
 */
struct BaseLimits
{
    /**
     * @brief The most fields one message may hold that its schema does not define. An undefined
     * value of a closed enum counts as one.
     *
     * Each nested message is counted on its own, so a later schema that adds a field to a message
     * repeated many times stays within the limit; the payload's size and its repeated-field caps
     * bound the total. @c maxUnknownFieldBytes bounds the whole payload.
     */
    uint64_t maxUnknownFields{16};

    /**
     * @brief The most bytes the unknown fields of the whole payload may take together.
     */
    uint64_t maxUnknownFieldBytes{1024};

    /**
     * @brief The deepest messages may nest.
     */
    uint64_t maxDepth{100};

    /**
     * @brief Whether a protobuf group (wire types 3 and 4) is refused.
     */
    bool rejectGroups{true};

    constexpr bool
    operator==(BaseLimits const&) const = default;
};

}  // namespace admission::spec
