#include <admissionspec/AdmissionSpec.h>
#include <admissionspec/Types.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

struct FooMessage
{
    size_t foo{0};
};

consteval auto
admissionSpec(std::type_identity<FooMessage>)
{
    return makeSpec<FooMessage>(
               admission::spec::tunable<"max_payload_bytes">(
                   std::uint64_t{64 * 1024}, "admission.foo.max_payload_bytes"),
               admission::spec::tunable<"size_ramp">(
                   admission::spec::ramp(
                       {{.upToBytes = 1024, .cost = 0.5}, {.upToBytes = 64 * 1024, .cost = 4.0}}),
                   "admission.foo.size_ramp"),
               admission::spec::tunable<"min_header_bytes">(
                   std::size_t{4}, "admission.foo.min_header_bytes"),
               admission::spec::tunable<"max_foo_value">(
                   std::size_t{100}, "admission.foo.max_foo_value"))
        .withPreCheck(
            [](std::span<std::byte const> bytes,
               auto const& cfg) -> admission::spec::AdmissionDecision {
                if (bytes.size() < cfg.template get<"min_header_bytes">())
                {
                    return admission::spec::AdmissionDecision::drop(
                        "payload too small to contain a header", 10.0);
                }
                // The user can apply additional token cost here for this admit result.
                return admission::spec::AdmissionDecision::admit();
            })
        .withPostCheck(
            [](FooMessage const& m, auto const& cfg) -> admission::spec::AdmissionDecision {
                if (m.foo > cfg.template get<"max_foo_value">())
                {
                    return admission::spec::AdmissionDecision::drop("foo value is invalid", 25.0);
                }
                // The user can apply additional token cost here for this admit result.
                return admission::spec::AdmissionDecision::admit();
            });
};

TEST(AdmissionSpecTests, PrePostAdmit)
{
    auto spec = admission::spec::admissionSpecFor<FooMessage>();
    auto resolved = spec.resolveDefaults();

    {
        auto tooSmall = std::array<std::byte, 3>{};
        auto decision = spec.preAdmit(tooSmall, resolved);
        EXPECT_EQ(decision.tokenCost, 10.0);
        EXPECT_EQ(decision.reason, "payload too small to contain a header");
        EXPECT_FALSE(decision.admitted());
        EXPECT_TRUE(decision.dropped());
    }

    {
        auto tooLarge = std::array<std::byte, (64 * 1024) + 1>{};
        auto decision = spec.preAdmit(tooLarge, resolved);
        EXPECT_EQ(decision.tokenCost, 4.0);
        EXPECT_EQ(decision.reason, "payload exceeds max bytes for this type");
        EXPECT_FALSE(decision.admitted());
        EXPECT_TRUE(decision.dropped());
    }

    {
        auto goldilocks = std::array<std::byte, 5>{};
        auto decision = spec.preAdmit(goldilocks, resolved);
        EXPECT_EQ(decision.tokenCost, 0.5);
        EXPECT_TRUE(decision.admitted());
        EXPECT_FALSE(decision.dropped());
    }

    {
        auto goldilocks = std::array<std::byte, 1025>{};
        auto decision = spec.preAdmit(goldilocks, resolved);
        EXPECT_EQ(decision.tokenCost, 4.0);
        EXPECT_TRUE(decision.admitted());
        EXPECT_FALSE(decision.dropped());
    }

    {
        auto foo = FooMessage{};
        foo.foo = 1000;
        auto decision = spec.postAdmit(foo, resolved);
        EXPECT_EQ(decision.tokenCost, 25.0);
        EXPECT_EQ(decision.reason, "foo value is invalid");
        EXPECT_FALSE(decision.admitted());
        EXPECT_TRUE(decision.dropped());
    }

    {
        auto foo = FooMessage{};
        foo.foo = 10;
        auto decision = spec.postAdmit(foo, resolved);
        EXPECT_EQ(decision.tokenCost, 0.0);
        EXPECT_TRUE(decision.admitted());
        EXPECT_FALSE(decision.dropped());
    }
}
