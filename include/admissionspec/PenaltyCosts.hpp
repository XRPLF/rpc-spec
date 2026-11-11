/** @file */
#pragma once

#include <admissionspec/PenaltyClass.hpp>

namespace admission::spec {

/**
 * @brief The cost of each penalty class, in a sink's units.
 *
 * There are no defaults: what a class costs is the host's policy.
 */
struct PenaltyCosts
{
    double trivial{};
    double useless{};
    double invalid{};
    double malformed{};

    /**
     * @return The cost of @p penalty.
     */
    [[nodiscard]] constexpr double
    operator()(PenaltyClass penalty) const noexcept
    {
        switch (penalty)
        {
            case PenaltyClass::None:
                return 0.0;
            case PenaltyClass::Trivial:
                return trivial;
            case PenaltyClass::Useless:
                return useless;
            case PenaltyClass::Invalid:
                return invalid;
            case PenaltyClass::Malformed:
                return malformed;
        }
        return malformed;
    }
};

}  // namespace admission::spec
