/** @file */
#pragma once

#include <cstdint>

namespace admission::spec {

/**
 * @brief How severely a refusal is penalized.
 *
 * A spec names the class; the host maps each class to its own charge. The mapping belongs to the
 * host's code, not to a spec or to configuration, since it decides when an honest sender is
 * disconnected.
 */
enum class PenaltyClass : uint8_t {
    None,       ///< Not penalized beyond the stage's cost.
    Trivial,    ///< What any message costs: for data an older release passes on as received.
    Useless,    ///< Data the host has no use for, such as a payload over its type's cap.
    Invalid,    ///< Data that breaks a rule every conforming sender follows.
    Malformed,  ///< Data no honest sender produces, such as a frame that cannot be decoded.
};

}  // namespace admission::spec
