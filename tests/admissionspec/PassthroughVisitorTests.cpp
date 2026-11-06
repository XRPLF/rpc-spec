#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/PassthroughVisitor.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <span>

using admission::spec::AdmissionDecision;
using admission::spec::EventKind;
using admission::spec::VisitEvent;

// The degenerate visitor: no decoding at all, the payload arrives as one opaque Bytes event.
TEST(PassthroughVisitor, emits_whole_payload_as_one_scalar)
{
    using admission::spec::visitPassthrough;

    auto const payload = std::array<uint8_t, 3>{0xDE, 0xAD, 0xBE};
    auto seen = 0;
    auto record = [&](VisitEvent const& event) {
        ++seen;
        EXPECT_EQ(event.kind, EventKind::Bytes);
        EXPECT_TRUE(event.key.empty());
        EXPECT_EQ(event.fieldNumber, std::numeric_limits<uint64_t>::max());
        auto const* value = event.as<std::span<uint8_t const>>();
        EXPECT_NE(value, nullptr);
        EXPECT_EQ(value->size(), payload.size());
        return AdmissionDecision::drop("nope");
    };

    auto const decision = visitPassthrough(std::span<uint8_t const>{payload}, record);
    EXPECT_EQ(seen, 1);
    EXPECT_FALSE(decision.admitted());
    EXPECT_EQ(decision.reason, "nope");
}
