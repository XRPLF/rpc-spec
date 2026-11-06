/** @file */
#pragma once

#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/RepeatedCaps.hpp>

#include <cstdint>
#include <optional>

namespace admission::spec {

/**
 * @brief The fixed facts about a message type that the admission stages act on.
 *
 * Unlike tunables, these are not configuration: they are properties of the protocol.
 */
struct SpecAttributes
{
    /**
     * @brief The type number a frame header names for this type. Required for @ref SpecRegistry.
     */
    std::optional<uint32_t> typeId;

    /**
     * @brief Whether only a sender in the host's cluster may send it.
     */
    bool fromClusterOnly{};

    /**
     * @brief A feature the connection must have negotiated. Set it only when every supported
     * sender checks the same feature before sending, or honest senders are refused.
     */
    std::optional<FeatureId> requiresFeature;

    /**
     * @brief Whether a supported older release passes this type on as received. If so, a refusal
     * for its size or content costs only @ref PenaltyClass::Trivial, so an honest relay is not
     * penalized for what it did not write.
     */
    bool relayedByOlderVersions{};

    /**
     * @brief Caps on repeated fields, checked while the payload is walked.
     */
    RepeatedCaps repeated{};

    constexpr bool
    operator==(SpecAttributes const&) const = default;
};

}  // namespace admission::spec
