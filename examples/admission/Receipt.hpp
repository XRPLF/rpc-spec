/** @file */
#pragma once

#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/Rule.hpp>

#include <cstdint>
#include <string_view>

namespace example {

/**
 * @brief What became of one message: a peer frame or an RPC request.
 */
struct Receipt
{
    enum class Outcome : uint8_t {
        Handled,  ///< Admitted, parsed and handled.
        Refused,  ///< Refused; its payload skipped, the connection kept.
        Closed,   ///< Refused, and the connection closed.
    };

    Outcome outcome{Outcome::Handled};
    /**
     * @brief The stage that decided: "header", "body", or "parse".
     */
    std::string_view stage;
    admission::spec::Rule rule{admission::spec::Rule::None};
    admission::spec::PenaltyClass penalty{admission::spec::PenaltyClass::None};
    std::string_view reason;
};

}  // namespace example
