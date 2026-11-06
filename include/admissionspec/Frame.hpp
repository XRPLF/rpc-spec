/** @file */
#pragma once

#include <concepts>
#include <cstdint>

namespace admission::spec {

/**
 * @brief What a frame's header declares, before any of its payload is read.
 *
 * The first admission stage decides from this alone, so a refused frame is never buffered. A peer
 * protocol fills it from its message header; RPC fills it from `Content-Length` or the WebSocket
 * frame header, with @c type naming the method.
 */
struct FrameInfo
{
    /**
     * @brief The message type, or the method id, the frame names.
     */
    uint32_t type{};

    /**
     * @brief Payload bytes on the wire.
     */
    uint64_t wireBytes{};

    /**
     * @brief Payload bytes once decoded. Equal to @c wireBytes when the payload is not compressed.
     */
    uint64_t decodedBytes{};

    /**
     * @brief Whether the payload is compressed.
     */
    bool compressed{};

    constexpr bool
    operator==(FrameInfo const&) const = default;
};

/**
 * @brief A protocol feature a connection may negotiate. Its values belong to the host.
 */
using FeatureId = uint32_t;

/**
 * @brief Where a sender stands with the host's charging, before or after a charge.
 *
 * @c Drop means the host should stop serving the sender. What that means (refuse the message, close
 * the connection) is the host's decision; the admission stages never close anything.
 */
enum class Standing : uint8_t { Ok, Warn, Drop };

/**
 * @brief The worse of two standings.
 */
[[nodiscard]] constexpr Standing
worse(Standing a, Standing b) noexcept
{
    return a > b ? a : b;
}

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
