/** @file */
#pragma once

#include <admissionspec/PenaltyClass.hpp>

#include <cstdint>
#include <string_view>

namespace admission::spec {

/**
 * @brief Outcome of an admission stage: admit (with a token cost) or drop (with a reason).
 */
enum class AdmissionAction : uint8_t { Admit, Drop };

/**
 * @brief The result an admission stage produces.
 *
 * @c tokenCost is the number of tokens the caller should debit from the connection's bucket. On an
 * admit it is the processing cost of the message; on a drop it is a penalty debited even though the
 * message is rejected, so cheap-to-generate rejects (oversize, malformed) still apply backpressure
 * instead of being free to spam. @c reason carries a human-readable explanation for a drop and is
 * empty when admitted. Stages never debit a bucket themselves — they only compute the cost — so
 * bucket exhaustion is a separate drop applied at the call site.
 */
struct AdmissionDecision
{
    /**
     * @brief Whether the message is admitted or dropped.
     */
    AdmissionAction action{AdmissionAction::Admit};

    /**
     * @brief Tokens debited from the connection's bucket.
     */
    double tokenCost{0.0};

    /**
     * @brief Human-readable reason, set on a drop.
     */
    std::string_view reason;

    /**
     * @brief How a refusal is penalized, on top of @c tokenCost. @c None when admitted.
     */
    PenaltyClass penalty{PenaltyClass::None};

    /**
     * @brief Whether the message was admitted.
     *
     * @return true when admitted; false otherwise.
     */
    [[nodiscard]] constexpr bool
    admitted() const noexcept
    {
        return action == AdmissionAction::Admit;
    }

    /**
     * @brief Whether the message was dropped.
     *
     * @return true when dropped; false otherwise.
     */
    [[nodiscard]] constexpr bool
    dropped() const noexcept
    {
        return action == AdmissionAction::Drop;
    }

    /**
     * @brief Construct an "admit" decision with the given token cost.
     *
     * @param cost Tokens to debit from the connection's bucket.
     * @return The admit decision.
     */
    [[nodiscard]] static constexpr AdmissionDecision
    admit(double cost = 0.0) noexcept
    {
        return {.action = AdmissionAction::Admit, .tokenCost = cost, .reason = {}};
    }

    /**
     * @brief Construct a "drop" decision, optionally charging a token penalty.
     *
     * @p cost is debited from the connection's bucket as a penalty even though the message is
     * rejected, so cheap-to-generate rejects (oversize, malformed) still apply backpressure instead
     * of being free to spam. Defaults to 0 for callers that do not penalize.
     *
     * @param why Human-readable reason, surfaced on the decision.
     * @param cost Penalty tokens to debit despite the rejection.
     * @return The drop decision.
     */
    [[nodiscard]] static constexpr AdmissionDecision
    drop(std::string_view why, double cost = 0.0) noexcept
    {
        return {.action = AdmissionAction::Drop, .tokenCost = cost, .reason = why};
    }

    /**
     * @brief Construct a "drop" decision with a penalty class.
     *
     * @param why Why the message is dropped.
     * @param penalty How the refusal is penalized; the host maps it to a charge.
     * @param cost The stage's cost, charged as well, so a refusal costs at least what admitting a
     *        message of the same size would.
     */
    [[nodiscard]] static constexpr AdmissionDecision
    drop(std::string_view why, PenaltyClass penalty, double cost = 0.0) noexcept
    {
        return {
            .action = AdmissionAction::Drop, .tokenCost = cost, .reason = why, .penalty = penalty};
    }

    /**
     * @brief Compare two values of this type.
     *
     * @return The comparison result.
     */
    bool
    operator==(AdmissionDecision const&) const = default;
};

}  // namespace admission::spec
