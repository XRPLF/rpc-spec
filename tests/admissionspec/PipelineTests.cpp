#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/BothSinks.hpp>
#include <admissionspec/BucketSettings.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/ConnectionLimiter.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/FrameInfo.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/PenaltyCosts.hpp>
#include <admissionspec/Pipeline.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/SpecAttributes.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/SpecRegistry.hpp>
#include <admissionspec/Standing.hpp>
#include <admissionspec/TokenBucket.hpp>
#include <admissionspec/TokenBucketSink.hpp>
#include <admissionspec/Tunable.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace admission::spec;

namespace {

constexpr FeatureId kReplayFeature = 2;

// A type with a 1 KiB cap, priced 1 per message up to 256 bytes and 4 above.
struct Small
{
};

consteval auto
admissionSpec(std::type_identity<Small>)
{
    return makeSpec<Small>(
               tunable<"max_payload_bytes">(uint64_t{1024}, "admission.small.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 256, .cost = 1.0}, {.upToBytes = 1024, .cost = 4.0}}),
                   "admission.small.size_ramp"))
        .withAttributes({.typeId = 3});
}

// Only the cluster may send it.
struct ClusterOnly
{
};

consteval auto
admissionSpec(std::type_identity<ClusterOnly>)
{
    return makeSpec<ClusterOnly>(
               tunable<"max_payload_bytes">(uint64_t{1024}, "admission.cluster.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 1024, .cost = 1.0}}), "admission.cluster.size_ramp"))
        .withAttributes({.typeId = 5, .fromClusterOnly = true});
}

// Needs a negotiated feature.
struct Gated
{
};

consteval auto
admissionSpec(std::type_identity<Gated>)
{
    return makeSpec<Gated>(
               tunable<"max_payload_bytes">(uint64_t{1024}, "admission.gated.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 1024, .cost = 1.0}}), "admission.gated.size_ramp"))
        .withAttributes({.typeId = 7, .requiresFeature = kReplayFeature});
}

// Passed on as received by older releases, with caps on two repeated fields.
struct Relayed
{
};

consteval auto
admissionSpec(std::type_identity<Relayed>)
{
    return makeSpec<Relayed>(
               tunable<"max_payload_bytes">(uint64_t{64}, "admission.relayed.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 64, .cost = 2.0}}), "admission.relayed.size_ramp"))
        .withAttributes(
            {.typeId = 9,
             .relayedByOlderVersions = true,
             .repeated = repeatedCaps(
                 {{.fieldNumber = 3, .maxEntries = 12, .minEntryBytes = 1},
                  {.fieldNumber = 4, .maxEntries = 2, .minEntryBytes = 32}})});
}

struct Connection
{
    bool cluster{};
    bool compression{};
    std::vector<FeatureId> features;

    [[nodiscard]] bool
    inCluster() const
    {
        return cluster;
    }

    [[nodiscard]] bool
    hasFeature(FeatureId feature) const
    {
        return std::ranges::find(features, feature) != features.end();
    }

    [[nodiscard]] bool
    compressionNegotiated() const
    {
        return compression;
    }
};

static_assert(ConnectionView<Connection>);

// Records each charge, and reports a fixed standing.
struct RecordingSink
{
    Standing current{Standing::Ok};
    std::vector<AdmissionDecision> charges;

    [[nodiscard]] Standing
    standing() const
    {
        return current;
    }

    Standing
    charge(AdmissionDecision const& decision)
    {
        charges.push_back(decision);
        return current;
    }
};

static_assert(ChargeSink<RecordingSink>);
static_assert(ChargeSink<TokenBucketSink>);
static_assert(ChargeSink<BothSinks<RecordingSink, TokenBucketSink>>);
static_assert(ChargeSink<ConnectionLimiter<int32_t>::ConnectionSink>);

constexpr auto kSmall = admissionSpecFor<Small>();
constexpr auto kRelayed = admissionSpecFor<Relayed>();

}  // namespace

TEST(AdmissionHeaderTests, admits_within_the_cap_at_the_ramp_cost_of_the_decoded_size)
{
    auto const decision = kSmall.admitHeader(
        FrameInfo{.type = 3, .wireBytes = 100, .decodedBytes = 300, .compressed = true},
        Connection{.compression = true},
        resolvedFor<Small>());
    EXPECT_TRUE(decision.admitted());
    EXPECT_DOUBLE_EQ(decision.tokenCost, 4.0);
    EXPECT_EQ(decision.penalty, PenaltyClass::None);
}

TEST(AdmissionHeaderTests, refuses_either_declared_size_over_the_cap)
{
    auto const connection = Connection{.compression = true};
    for (auto const& frame : {
             FrameInfo{.wireBytes = 1025, .decodedBytes = 1025},
             FrameInfo{.wireBytes = 64, .decodedBytes = 1025, .compressed = true},
         })
    {
        auto const decision = kSmall.admitHeader(frame, connection, resolvedFor<Small>());
        EXPECT_TRUE(decision.dropped());
        EXPECT_EQ(decision.penalty, PenaltyClass::Useless);
        // A refusal costs what admitting the same size would.
        EXPECT_GT(decision.tokenCost, 0.0);
    }
}

TEST(AdmissionHeaderTests, refuses_a_compressed_payload_without_negotiated_compression)
{
    auto const decision = kSmall.admitHeader(
        FrameInfo{.wireBytes = 10, .decodedBytes = 20, .compressed = true},
        Connection{.compression = false},
        resolvedFor<Small>());
    EXPECT_TRUE(decision.dropped());
    EXPECT_EQ(decision.penalty, PenaltyClass::Malformed);
}

TEST(AdmissionHeaderTests, refuses_a_cluster_only_type_from_outside_the_cluster)
{
    constexpr auto kSpec = admissionSpecFor<ClusterOnly>();
    auto const frame = FrameInfo{.wireBytes = 10, .decodedBytes = 10};
    EXPECT_EQ(
        kSpec.admitHeader(frame, Connection{.cluster = false}, resolvedFor<ClusterOnly>()).penalty,
        PenaltyClass::Useless);
    EXPECT_TRUE(kSpec.admitHeader(frame, Connection{.cluster = true}, resolvedFor<ClusterOnly>())
                    .admitted());
}

TEST(AdmissionHeaderTests, refuses_a_gated_type_without_its_feature)
{
    constexpr auto kSpec = admissionSpecFor<Gated>();
    auto const frame = FrameInfo{.wireBytes = 10, .decodedBytes = 10};
    EXPECT_TRUE(kSpec.admitHeader(frame, Connection{}, resolvedFor<Gated>()).dropped());
    EXPECT_TRUE(
        kSpec.admitHeader(frame, Connection{.features = {kReplayFeature}}, resolvedFor<Gated>())
            .admitted());
}

TEST(AdmissionHeaderTests, a_relayed_type_is_refused_at_the_trivial_penalty_only)
{
    auto const decision = kRelayed.admitHeader(
        FrameInfo{.wireBytes = 65, .decodedBytes = 65}, Connection{}, resolvedFor<Relayed>());
    EXPECT_TRUE(decision.dropped());
    EXPECT_EQ(decision.penalty, PenaltyClass::Trivial);
    EXPECT_DOUBLE_EQ(decision.tokenCost, 0.0);
    EXPECT_EQ(kRelayed.penaltyFor(PenaltyClass::Malformed), PenaltyClass::Trivial);
    EXPECT_EQ(kSmall.penaltyFor(PenaltyClass::Malformed), PenaltyClass::Malformed);
}

TEST(AdmissionHeaderTests, attributes_carry_repeated_caps)
{
    auto const& attributes = kRelayed.attributes();
    ASSERT_TRUE(attributes.repeated.find(4).has_value());
    EXPECT_EQ(attributes.repeated.find(4)->minEntryBytes, 32U);
    EXPECT_FALSE(attributes.repeated.find(5).has_value());
}

TEST(AdmissionPipelineTests, charges_the_header_stage_admitted_or_not)
{
    auto sink = RecordingSink{};
    auto const admitted =
        admitHeader<Small>(FrameInfo{.wireBytes = 10, .decodedBytes = 10}, Connection{}, sink);
    auto const refused =
        admitHeader<Small>(FrameInfo{.wireBytes = 2048, .decodedBytes = 2048}, Connection{}, sink);
    EXPECT_TRUE(admitted.decision.admitted());
    EXPECT_TRUE(refused.decision.dropped());
    ASSERT_EQ(sink.charges.size(), 2U);
    EXPECT_EQ(sink.charges[0].penalty, PenaltyClass::None);
    EXPECT_EQ(sink.charges[1].penalty, PenaltyClass::Useless);
}

TEST(AdmissionPipelineTests, a_sender_over_its_limit_is_refused_unread_and_not_charged_again)
{
    auto sink = RecordingSink{.current = Standing::Drop};
    auto const result =
        admitHeader<Small>(FrameInfo{.wireBytes = 10, .decodedBytes = 10}, Connection{}, sink);
    EXPECT_TRUE(result.decision.dropped());
    EXPECT_EQ(result.standing, Standing::Drop);
    EXPECT_TRUE(sink.charges.empty());
}

TEST(AdmissionPipelineTests, a_body_refusal_is_charged_the_penalty_its_spec_allows)
{
    auto const refuse = [](auto&) {
        return AdmissionDecision::drop("too many entries", PenaltyClass::Invalid, 50.0);
    };

    // A type older releases relay: capped at the trivial penalty, with no cost.
    auto relayed = RecordingSink{};
    auto const capped = admitBody<Relayed>(refuse, relayed);
    EXPECT_EQ(capped.decision.penalty, PenaltyClass::Trivial);
    ASSERT_EQ(relayed.charges.size(), 1U);
    EXPECT_DOUBLE_EQ(relayed.charges[0].tokenCost, 0.0);

    // Any other type: the penalty and cost the walk asked for.
    auto other = RecordingSink{};
    auto const full = admitBody<Small>(refuse, other);
    EXPECT_EQ(full.decision.penalty, PenaltyClass::Invalid);
    EXPECT_DOUBLE_EQ(full.decision.tokenCost, 50.0);

    // An admission is charged only a cost the walk asked for.
    auto quiet = RecordingSink{};
    EXPECT_TRUE(
        admitBody<Small>([](auto&) { return AdmissionDecision::admit(); }, quiet)
            .decision.admitted());
    EXPECT_TRUE(quiet.charges.empty());
}

TEST(ChargeSinkTests, token_bucket_sink_takes_the_larger_of_cost_and_penalty)
{
    auto const now = TokenBucket::Clock::now();
    // Every call uses the same time, so nothing refills between them.
    auto bucket = TokenBucket{100.0, 1.0, now};
    auto sink = TokenBucketSink{
        bucket, now, PenaltyCosts{.trivial = 1, .useless = 10, .invalid = 40, .malformed = 80}};

    EXPECT_EQ(sink.charge(AdmissionDecision::admit(5.0)), Standing::Ok);
    EXPECT_DOUBLE_EQ(bucket.available(now), 95.0);
    EXPECT_EQ(sink.charge(AdmissionDecision::drop("r", PenaltyClass::Invalid, 5.0)), Standing::Ok);
    EXPECT_DOUBLE_EQ(bucket.available(now), 55.0);

    // A charge the bucket cannot cover takes nothing.
    EXPECT_EQ(sink.charge(AdmissionDecision::drop("r", PenaltyClass::Malformed)), Standing::Drop);
    EXPECT_DOUBLE_EQ(bucket.available(now), 55.0);
    EXPECT_EQ(sink.standing(), Standing::Ok);
}

TEST(ChargeSinkTests, both_sinks_charges_both_and_reports_the_worse)
{
    auto first = RecordingSink{.current = Standing::Warn};
    auto second = RecordingSink{.current = Standing::Ok};
    auto both = BothSinks{first, second};
    EXPECT_EQ(both.standing(), Standing::Warn);
    EXPECT_EQ(both.charge(AdmissionDecision::admit(1.0)), Standing::Warn);
    EXPECT_EQ(first.charges.size(), 1U);
    EXPECT_EQ(second.charges.size(), 1U);
}

TEST(SpecRegistryTests, dispatches_by_type_number)
{
    using Registry = SpecRegistry<Small, ClusterOnly, Gated, Relayed>;
    auto const name = [](uint32_t type) {
        return Registry::dispatch(
            type,
            []<class T> { return std::string_view{std::is_same_v<T, Gated> ? "gated" : "other"}; },
            [] { return std::string_view{"unknown"}; });
    };
    EXPECT_EQ(name(7), "gated");
    EXPECT_EQ(name(3), "other");
    EXPECT_EQ(name(42), "unknown");

    auto visited = 0;
    Registry::dispatch(9, [&]<class T> { ++visited; }, [&] { visited = -1; });
    Registry::dispatch(42, [&]<class T> { ++visited; }, [&] { visited += 10; });
    EXPECT_EQ(visited, 11);
}

TEST(SpecRegistryTests, runs_the_header_stage_for_the_named_type)
{
    using Registry = SpecRegistry<Small, Relayed>;
    auto sink = RecordingSink{};
    auto const frame = FrameInfo{.type = 9, .wireBytes = 100, .decodedBytes = 100};
    auto const result = Registry::dispatch(
        frame.type,
        [&]<class T> { return admitHeader<T>(frame, Connection{}, sink); },
        [] {
            return StageResult{
                .decision = AdmissionDecision::drop("unknown type", PenaltyClass::Useless)};
        });
    EXPECT_TRUE(result.decision.dropped());
    EXPECT_EQ(result.decision.penalty, PenaltyClass::Trivial);
}

TEST(ChargeSinkTests, a_limiter_hands_out_a_sink_per_connection)
{
    auto const now = TokenBucket::Clock::now();
    auto limiter =
        ConnectionLimiter<int32_t>{BucketSettings{.capacity = 10.0, .refillRatePerSecond = 1.0}, 4};
    auto const costs = PenaltyCosts{.trivial = 1, .useless = 5, .invalid = 5, .malformed = 10};

    // Each small message costs 1 token, by Small's ramp, so the eleventh is refused.
    auto first = limiter.sink(1, now, costs);
    auto const frame = FrameInfo{.wireBytes = 10, .decodedBytes = 10};
    for (int32_t i{}; i < 10; ++i)
    {
        EXPECT_EQ(admitHeader<Small>(frame, Connection{}, first).standing, Standing::Ok);
    }
    EXPECT_EQ(admitHeader<Small>(frame, Connection{}, first).standing, Standing::Drop);
    EXPECT_EQ(first.standing(), Standing::Drop);

    // Another connection has its own bucket.
    auto second = limiter.sink(2, now, costs);
    EXPECT_EQ(admitHeader<Small>(frame, Connection{}, second).standing, Standing::Ok);
    EXPECT_EQ(limiter.size(), 2U);

    // The bucket refills with time.
    auto later = limiter.sink(1, now + std::chrono::seconds{2}, costs);
    EXPECT_EQ(later.standing(), Standing::Ok);
}
