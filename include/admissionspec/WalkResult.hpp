/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/PayloadTotals.hpp>

namespace admission::spec {

/**
 * @brief What a walker that counts returns: its decision, and what it counted on the way.
 *
 * A walker passed to @ref admitBody may return this or a bare @ref AdmissionDecision.
 */
struct WalkResult
{
    AdmissionDecision decision;
    PayloadTotals totals;
};

}  // namespace admission::spec
