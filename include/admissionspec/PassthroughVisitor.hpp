#pragma once

#include <admissionspec/Types.hpp>

#include <span>

namespace admission::spec {

/**
 * @brief Implements a visitor that will just simply pass the message payload onto the `check`
 * function.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
visitPassthrough(std::span<std::uint8_t const> bytes, Check& check)
{
    return check(VisitEvent{.kind = EventKind::Scalar, .fieldNumber = field, .value = bytes});
}

}  // namespace admission::spec
