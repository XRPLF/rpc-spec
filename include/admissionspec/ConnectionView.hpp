/** @file */
#pragma once

#include <concepts>
#include <cstdint>

namespace admission::spec {

/**
 * @brief A protocol feature a connection may negotiate. Its values belong to the host.
 */
using FeatureId = uint32_t;

/**
 * @brief What the admission stages may ask about the connection a frame arrived on.
 *
 * The host implements it; the stages never see the host's connection type.
 */
template <typename C>
concept ConnectionView = requires(C const& connection, FeatureId feature) {
    { connection.inCluster() } -> std::same_as<bool>;
    { connection.hasFeature(feature) } -> std::same_as<bool>;
    { connection.compressionNegotiated() } -> std::same_as<bool>;
};

}  // namespace admission::spec
