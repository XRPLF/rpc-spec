/** @file */
#pragma once

// A gossip node's receive path, built on the admission stages.
//
// What the framework leaves to the host, and this file supplies:
// - what the stages may ask about a connection (PeerConnection, a ConnectionView);
// - where charges go (ReputationSink, the node's lasting record of a peer, combined with a
//   TokenBucketSink that limits how fast one connection may send);
// - the framing: reading a frame's header, skipping a refused payload, parsing an admitted one;
// - what to do about a sender at Drop.

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AllSinks.hpp>
#include <admissionspec/ChargeSink.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/FrameInfo.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/PenaltyCosts.hpp>
#include <admissionspec/Pipeline.hpp>
#include <admissionspec/ProtobufPrescan.hpp>
#include <admissionspec/Rule.hpp>
#include <admissionspec/Standing.hpp>
#include <admissionspec/TokenBucket.hpp>
#include <admissionspec/TokenBucketSink.hpp>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message_lite.h>

#include <GossipSpecs.hpp>
#include <Receipt.hpp>
#include <ReputationSink.hpp>
#include <gossip.pb.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace example {

/**
 * @brief What the admission stages may ask about one peer connection.
 */
struct PeerConnection
{
    bool trusted{};
    std::vector<admission::spec::FeatureId> features;

    [[nodiscard]] bool
    privileged() const
    {
        return trusted;
    }

    [[nodiscard]] bool
    hasFeature(admission::spec::FeatureId feature) const
    {
        return std::ranges::find(features, feature) != features.end();
    }

    [[nodiscard]] bool
    compressionNegotiated() const
    {
        return false;
    }
};

static_assert(admission::spec::ConnectionView<PeerConnection>);

/**
 * @brief A frame: a 2-byte type and a 4-byte payload size, big-endian, then the payload.
 */
inline constexpr size_t kHeaderBytes{6};

/**
 * @brief A frame of type @p type around @p payload.
 */
[[nodiscard]] inline std::vector<uint8_t>
frame(uint16_t type, std::span<uint8_t const> payload)
{
    auto const size = static_cast<uint32_t>(payload.size());
    auto bytes = std::vector<uint8_t>{
        static_cast<uint8_t>(type >> 8),
        static_cast<uint8_t>(type),
        static_cast<uint8_t>(size >> 24),
        static_cast<uint8_t>(size >> 16),
        static_cast<uint8_t>(size >> 8),
        static_cast<uint8_t>(size)};
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

/**
 * @brief A frame holding @p message.
 */
[[nodiscard]] inline std::vector<uint8_t>
frame(MessageType type, google::protobuf::MessageLite const& message)
{
    auto const payload = message.SerializeAsString();
    return frame(
        static_cast<uint16_t>(type),
        std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(payload.data()), payload.size()});
}

/**
 * @brief One peer connection's receive path.
 */
class GossipHost
{
    // An admitted message whose charge left the sender at Drop.
    static constexpr auto kOverLimit = admission::spec::AdmissionDecision::drop(
        admission::spec::Rule::Sender,
        "sender is over its limit",
        admission::spec::PenaltyClass::None);

    PeerConnection connection_;
    ReputationSink reputation_;
    admission::spec::TokenBucket bucket_;
    bool closed_{};
    std::vector<std::string> handled_;

public:
    /**
     * @param connection The peer's connection.
     * @param burst How many tokens the connection may spend at once.
     * @param refillPerSecond How fast its tokens come back.
     */
    GossipHost(PeerConnection connection, double burst, double refillPerSecond)
        : connection_{std::move(connection)}
        , bucket_{burst, refillPerSecond, admission::spec::TokenBucket::Clock::now()}
    {
    }

    /**
     * @brief Take in one whole frame.
     *
     * @param bytes The frame.
     * @param pieceBytes The size of the receive buffers the payload arrives in. The body stage
     *        walks the pieces as they are, without copying them into one.
     */
    Receipt
    receive(std::span<uint8_t const> bytes, int32_t pieceBytes)
    {
        if (closed_)
        {
            return {.outcome = Receipt::Outcome::Closed, .stage = "closed"};
        }
        if (bytes.size() < kHeaderBytes)
        {
            return close("header", admission::spec::Rule::Malformed, "truncated header");
        }

        // Stage 1: the header alone. Nothing of the payload has been read.
        auto const type = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
        auto const size = (uint32_t{bytes[2]} << 24) | (uint32_t{bytes[3]} << 16) |
            (uint32_t{bytes[4]} << 8) | uint32_t{bytes[5]};
        auto const info = admission::spec::FrameInfo{
            .type = type, .wireBytes = size, .decodedBytes = size, .compressed = false};

        // Both sinks are charged for every decision: the reputation lasts, the bucket refills.
        auto bucketSink = admission::spec::TokenBucketSink{
            bucket_,
            admission::spec::TokenBucket::Clock::now(),
            admission::spec::PenaltyCosts{
                .trivial = 1.0, .useless = 5.0, .invalid = 10.0, .malformed = 20.0}};
        auto sinks = admission::spec::AllSinks{reputation_, bucketSink};

        auto const header = GossipSpecs::dispatch(
            type,
            [&]<class T> { return admission::spec::admitHeader<T>(info, connection_, sinks); },
            [&] {
                // A type with no spec is the host's to price. Peers must not send a new type to a
                // node that did not announce it, so this is useless data.
                auto const refusal = admission::spec::AdmissionDecision::drop(
                    admission::spec::Rule::UnknownType,
                    "unknown message type",
                    admission::spec::PenaltyClass::Useless);
                return admission::spec::StageResult{
                    .decision = refusal, .standing = sinks.charge(refusal)};
            });
        if (header.decision.dropped())
        {
            // A real host skips the payload's bytes as they arrive, never buffering them.
            return refuse("header", header.decision);
        }
        // Admitted, but its charge took a sink to Drop: here, a bucket that could not cover it.
        // The host stops serving the sender, whatever the decision.
        if (header.standing == admission::spec::Standing::Drop)
        {
            return refuse("header", kOverLimit);
        }
        if (bytes.size() != kHeaderBytes + size)
        {
            return close("header", admission::spec::Rule::Malformed, "frame size mismatch");
        }

        // Stage 2: walk the payload, in the pieces it arrived in, before parsing it.
        auto const payload = bytes.subspan(kHeaderBytes);
        return GossipSpecs::dispatch(
            type,
            [&]<class T> {
                auto stream = google::protobuf::io::ArrayInputStream{
                    payload.data(), static_cast<int>(payload.size()), pieceBytes};
                auto const body = admission::spec::admitBody<T>(
                    admission::spec::protobufVisitor<T>(stream, payload.size()),
                    connection_,
                    sinks);
                if (body.decision.dropped())
                {
                    return refuse("body", body.decision);
                }
                if (body.standing == admission::spec::Standing::Drop)
                {
                    return refuse("body", kOverLimit);
                }

                // Stage 3: parse, now that the walk has shown the payload is within its limits.
                auto message = T{};
                if (not message.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
                {
                    // The walk accepts exactly what the parser does, so this does not happen.
                    return close("parse", admission::spec::Rule::Malformed, "did not parse");
                }
                handled_.push_back(std::string{T::GetDescriptor()->name()});
                return Receipt{.outcome = Receipt::Outcome::Handled, .stage = "body"};
            },
            [&] {
                return close("body", admission::spec::Rule::Malformed, "unknown past its header");
            });
    }

    [[nodiscard]] double
    reputation() const
    {
        return reputation_.balance();
    }

    [[nodiscard]] double
    tokens()
    {
        return bucket_.available(admission::spec::TokenBucket::Clock::now());
    }

    [[nodiscard]] std::vector<std::string> const&
    handled() const
    {
        return handled_;
    }

private:
    // The bucket only limits flow: running out refuses messages, but only the lasting record
    // reaching Drop, or a payload that cannot be read, closes the connection.
    Receipt
    refuse(std::string_view stage, admission::spec::AdmissionDecision const& decision)
    {
        if (reputation_.standing() == admission::spec::Standing::Drop or
            decision.rule == admission::spec::Rule::Malformed)
        {
            closed_ = true;
        }
        return {
            .outcome = closed_ ? Receipt::Outcome::Closed : Receipt::Outcome::Refused,
            .stage = stage,
            .rule = decision.rule,
            .penalty = decision.penalty,
            .reason = decision.reason};
    }

    Receipt
    close(std::string_view stage, admission::spec::Rule rule, std::string_view why)
    {
        closed_ = true;
        return {
            .outcome = Receipt::Outcome::Closed,
            .stage = stage,
            .rule = rule,
            .penalty = admission::spec::PenaltyClass::Malformed,
            .reason = why};
    }
};

}  // namespace example
