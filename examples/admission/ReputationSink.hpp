/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/Standing.hpp>

#include <algorithm>

namespace example {

/**
 * @brief The node's lasting record of a sender, a peer or an RPC client: a balance every charge
 * adds to.
 *
 * The fee for each penalty class is fixed here, in the host's code, not in a spec or in
 * configuration: it decides when an honest peer is disconnected, which must not differ from node
 * to node. A real host's balance decays over time and outlives the connection; this one only adds.
 */
class ReputationSink
{
    double balance_{};

public:
    static constexpr double kWarnAt{500.0};
    static constexpr double kDropAt{1000.0};

    [[nodiscard]] static constexpr double
    fee(admission::spec::PenaltyClass penalty) noexcept
    {
        switch (penalty)
        {
            case admission::spec::PenaltyClass::None:
                return 0.0;
            case admission::spec::PenaltyClass::Trivial:
                return 1.0;
            case admission::spec::PenaltyClass::Useless:
                return 50.0;
            case admission::spec::PenaltyClass::Invalid:
                return 200.0;
            case admission::spec::PenaltyClass::Malformed:
                return 1000.0;
        }
        return 1000.0;
    }

    [[nodiscard]] admission::spec::Standing
    standing() const
    {
        if (balance_ >= kDropAt)
        {
            return admission::spec::Standing::Drop;
        }
        return balance_ >= kWarnAt ? admission::spec::Standing::Warn
                                   : admission::spec::Standing::Ok;
    }

    /**
     * @brief Charge the larger of the stage's cost and the fee for its penalty, so a refusal never
     * costs less than admitting a message of the same size.
     */
    admission::spec::Standing
    charge(admission::spec::AdmissionDecision const& decision)
    {
        balance_ += std::max(decision.tokenCost, fee(decision.penalty));
        return standing();
    }

    [[nodiscard]] double
    balance() const
    {
        return balance_;
    }
};

static_assert(admission::spec::ChargeSink<ReputationSink>);

}  // namespace example
