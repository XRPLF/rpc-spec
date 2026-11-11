/** @file */
#pragma once

#include <cstdint>

namespace admission::spec {

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

}  // namespace admission::spec
