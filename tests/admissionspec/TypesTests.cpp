#include <admissionspec/Types.h>
#include <gtest/gtest.h>

#include <array>
#include <tuple>
#include <type_traits>
#include <vector>

TEST(TypesTests, AdmissionDecisionDefaultConstructed)
{
    constexpr auto decision = admission::spec::AdmissionDecision{};
    EXPECT_EQ(decision.admitted(), true);
    EXPECT_EQ(decision.dropped(), false);
    EXPECT_EQ(decision.action, admission::spec::AdmissionAction::Admit);
    EXPECT_EQ(decision.tokenCost, 0.0);
    EXPECT_TRUE(decision.reason.empty());
}

TEST(TypesTests, AdmissionDecisionAdmit)
{
    constexpr auto decision = admission::spec::AdmissionDecision::admit(42.5);
    EXPECT_EQ(decision.admitted(), true);
    EXPECT_EQ(decision.dropped(), false);
    EXPECT_EQ(decision.action, admission::spec::AdmissionAction::Admit);
    EXPECT_EQ(decision.tokenCost, 42.5);
    EXPECT_TRUE(decision.reason.empty());
}

TEST(TypesTests, AdmissionDecisionDrop)
{
    constexpr auto decision = admission::spec::AdmissionDecision::drop(
        "The quick brown fox jumps over the lazy dog.", 123.45);
    EXPECT_EQ(decision.admitted(), false);
    EXPECT_EQ(decision.dropped(), true);
    EXPECT_EQ(decision.action, admission::spec::AdmissionAction::Drop);
    EXPECT_EQ(decision.tokenCost, 123.45);
    EXPECT_FALSE(decision.reason.empty());
    EXPECT_EQ(decision.reason, std::string_view{"The quick brown fox jumps over the lazy dog."});
}

TEST(TypesTests, FixedString)
{
    constexpr auto fs =
        admission::spec::FixedString{"The quick brown fox jumps over the lazy dog."};
    EXPECT_FALSE(fs.view().empty());
    EXPECT_EQ(fs.view(), "The quick brown fox jumps over the lazy dog.");
}

TEST(TypesTests, SizeCostRamp)
{
    constexpr auto tiers = std::to_array<admission::spec::SizeTier>({
        {.upToBytes = 10, .cost = 1.23},
        {.upToBytes = 100, .cost = 10.23},
        {.upToBytes = 1000, .cost = 100.23},
        {.upToBytes = 10000, .cost = 1000.23},
    });
    constexpr auto ramp1 = admission::spec::SizeCostRamp(tiers);
    EXPECT_EQ(tiers, ramp1.tiers);

    constexpr auto ramp2 = admission::spec::ramp({
        admission::spec::SizeTier{.upToBytes = 10, .cost = 1.23},
        admission::spec::SizeTier{.upToBytes = 100, .cost = 10.23},
        admission::spec::SizeTier{.upToBytes = 1000, .cost = 100.23},
        admission::spec::SizeTier{.upToBytes = 10000, .cost = 1000.23},
    });
    EXPECT_EQ(tiers, ramp2.tiers);

    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 0), 1.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 5), 1.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 10), 1.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 11), 10.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 50), 10.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 100), 10.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 101), 100.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 500), 100.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 1000), 100.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 1001), 1000.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 5000), 1000.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 10000), 1000.23);
    EXPECT_EQ(admission::spec::costFor(ramp1.tiers, 1000000), 1000.23);
}

TEST(TypesTests, Tunable)
{
    constexpr auto t1 = admission::spec::tunable<"max_bytes">(10000ull, "test.max_bytes");
    EXPECT_EQ(t1.defaultValue, 10000ull);
    EXPECT_EQ(t1.configKey, "test.max_bytes");
    EXPECT_EQ(decltype(t1)::kNAME, "max_bytes");
    EXPECT_EQ(t1.defaultValue, admission::spec::toResolved(t1.defaultValue));
    if constexpr (!std::is_same_v<
                      decltype(t1)::ValueType,
                      admission::spec::ResolvedTypeOfT<decltype(t1)>>)
    {
        EXPECT_FALSE(false) << "T1 Tunable type mismatch";
    }

    constexpr auto t2 = admission::spec::tunable<"max_requests">(42.5, "test.max_requests");
    EXPECT_EQ(t2.defaultValue, 42.5);
    EXPECT_EQ(t2.configKey, "test.max_requests");
    EXPECT_EQ(decltype(t2)::kNAME, "max_requests");
    EXPECT_EQ(t2.defaultValue, admission::spec::toResolved(t2.defaultValue));
    if constexpr (!std::is_same_v<
                      decltype(t2)::ValueType,
                      admission::spec::ResolvedTypeOfT<decltype(t2)>>)
    {
        EXPECT_FALSE(false) << "T2 Tunable type mismatch";
    }

    constexpr auto ramp = admission::spec::ramp({
        admission::spec::SizeTier{.upToBytes = 10, .cost = 1.23},
        admission::spec::SizeTier{.upToBytes = 100, .cost = 10.23},
        admission::spec::SizeTier{.upToBytes = 1000, .cost = 100.23},
        admission::spec::SizeTier{.upToBytes = 10000, .cost = 1000.23},
    });
    constexpr auto t3 = admission::spec::tunable<"max_bytes_2">(ramp, "test.max_bytes_2");
    EXPECT_EQ(t3.defaultValue, ramp);
    EXPECT_EQ(t3.configKey, "test.max_bytes_2");
    EXPECT_EQ(decltype(t3)::kNAME, "max_bytes_2");
    auto resolved = std::vector<admission::spec::SizeTier>(
        t3.defaultValue.tiers.begin(), t3.defaultValue.tiers.end());
    EXPECT_EQ(resolved, admission::spec::toResolved(t3.defaultValue));
    if constexpr (!std::is_same_v<
                      decltype(t3)::ValueType,
                      admission::spec::ResolvedTypeOfT<decltype(t3)>>)
    {
        EXPECT_FALSE(false) << "T3 Tunable type mismatch";
    }
}

TEST(TypesTests, Tunables)
{
    constexpr auto t1 = admission::spec::tunable<"max_bytes">(10000ull, "test.max_bytes");
    constexpr auto t2 = admission::spec::tunable<"max_requests">(42.5, "test.max_requests");

    constexpr auto ramp = admission::spec::ramp({
        admission::spec::SizeTier{.upToBytes = 10, .cost = 1.23},
        admission::spec::SizeTier{.upToBytes = 100, .cost = 10.23},
        admission::spec::SizeTier{.upToBytes = 1000, .cost = 100.23},
        admission::spec::SizeTier{.upToBytes = 10000, .cost = 1000.23},
    });
    constexpr auto t3 = admission::spec::tunable<"max_bytes_2">(ramp, "test.max_bytes_2");

    constexpr auto tunables = std::make_tuple(t1, t2, t3);
    auto resolved = admission::spec::resolveTunableDefaults(tunables);

    EXPECT_TRUE(resolved.has<"max_bytes">());
    EXPECT_EQ(resolved.get<"max_bytes">(), t1.defaultValue);
    EXPECT_TRUE(resolved.has<"max_requests">());
    EXPECT_EQ(resolved.get<"max_requests">(), t2.defaultValue);
    EXPECT_TRUE(resolved.has<"max_bytes_2">());

    auto resolvedT3 = std::vector<admission::spec::SizeTier>(
        t3.defaultValue.tiers.begin(), t3.defaultValue.tiers.end());
    EXPECT_EQ(resolved.get<"max_bytes_2">(), resolvedT3);

    EXPECT_FALSE(resolved.has<"foo_bar">());
}
