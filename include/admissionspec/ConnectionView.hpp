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
 * The host implements it; the stages never see the host's connection type. `privileged()` is the
 * host's trust in the sender: membership of its cluster for a peer, the admin role for an RPC
 * client.
 */
template <typename C>
concept ConnectionView = requires(C const& connection, FeatureId feature) {
    { connection.privileged() } -> std::same_as<bool>;
    { connection.hasFeature(feature) } -> std::same_as<bool>;
    { connection.compressionNegotiated() } -> std::same_as<bool>;
};

}  // namespace admission::spec
