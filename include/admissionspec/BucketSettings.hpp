/** @file */
#pragma once

#include <admissionspec/Tunable.hpp>

namespace admission::spec {

/**
 * @brief Connection-scoped token-bucket parameters.
 *
 * There is exactly ONE bucket per connection, and a message type only affects the *cost* charged
 * against it — types never get their own bucket. These parameters therefore live at connection
 * scope (resolved once from config), and are intentionally NOT part of @ref AdmissionSpec.
 */
struct BucketParams
{
    /**
     * @brief Burst size, in tokens.
     */
    Tunable<"capacity", double> capacity;

    /**
     * @brief Tokens added back per second.
     */
    Tunable<"refill_rate_per_second", double> refillRatePerSecond;
};

/**
 * @brief The resolved (config-overridden) connection-bucket parameters the limiter is constructed
 * with.
 */
struct BucketSettings
{
    /**
     * @brief Burst size, in tokens.
     */
    double capacity{};

    /**
     * @brief Tokens added back per second.
     */
    double refillRatePerSecond{};
};

}  // namespace admission::spec
