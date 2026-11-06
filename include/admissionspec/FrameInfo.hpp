/** @file */
#pragma once

#include <cstdint>

namespace admission::spec {

/**
 * @brief What a frame's header declares, before any of its payload is read.
 *
 * The header stage decides from this alone, so a refused frame is never buffered. A peer protocol
 * fills it from its message header; RPC fills it from `Content-Length` or the WebSocket frame
 * header, with @c type naming the method.
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

}  // namespace admission::spec
