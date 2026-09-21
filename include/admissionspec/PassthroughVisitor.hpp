/** @file */
#pragma once

#include <admissionspec/Types.hpp>

#include <cstdint>
#include <span>

namespace admission::spec {

/**
 * @brief A visitor that hands the whole message payload to @p check as a single scalar event.
 *
 * The degenerate case of a visitor: it decodes nothing, so the emitted event carries no name
 * and no field number (`VisitEvent::fieldNumber` keeps its "not applicable" default). Use it
 * for opaque payloads whose only admission criterion is their bytes.
 *
 * @param bytes The raw message payload.
 * @param check The per-message check to invoke.
 * @return Whatever @p check decides.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
visitPassthrough(std::span<uint8_t const> bytes, Check& check)
{
    return check(VisitEvent{.kind = EventKind::Scalar, .value = bytes});
}

}  // namespace admission::spec
