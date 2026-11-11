/** @file */
#pragma once

#include <cstdint>

namespace admission::spec {

/**
 * @brief Which rule a refusal broke, so a host can act on it without reading the reason text.
 *
 * Several rules share a @ref PenaltyClass; a host that charges or reports them differently tells
 * them apart by this.
 */
enum class Rule : uint8_t {
    None,           ///< Not refused.
    Sender,         ///< The sender is already over its limit.
    UnknownType,    ///< The frame names a type with no spec (see @ref SpecRegistry).
    Privileged,     ///< Only a privileged sender may send the type.
    Feature,        ///< The connection did not negotiate the type's feature.
    Compression,    ///< A compressed payload on a connection that did not negotiate compression.
    Oversized,      ///< A declared size is over the type's cap.
    RepeatedCap,    ///< A repeated field holds more entries than its cap.
    EntrySize,      ///< A repeated field's entry is smaller than its cap allows.
    UnknownFields,  ///< More fields the schema does not define, or bytes in them, than allowed.
    Group,          ///< A protobuf group, where groups are refused.
    Depth,          ///< Messages nested deeper than allowed.
    Malformed,      ///< The payload does not decode.
    Check,          ///< The spec's own check refused it.
};

}  // namespace admission::spec
