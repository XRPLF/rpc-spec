/** @file */
#pragma once

// An RPC endpoint's request path, built on the same admission stages as GossipHost.
//
// Where it differs from the peer side:
// - the method comes from the request path, and the declared size from Content-Length;
// - the body is JSON, walked by jsonVisitor rather than the protobuf prescan;
// - "privileged" is the admin role rather than cluster membership;
// - a refusal answers the request with an error; it never closes anything by itself.

#include <boost/json/parse.hpp>
#include <boost/system/error_code.hpp>

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AllSinks.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/FrameInfo.hpp>
#include <admissionspec/JsonVisitor.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/PenaltyCosts.hpp>
#include <admissionspec/Pipeline.hpp>
#include <admissionspec/Rule.hpp>
#include <admissionspec/Standing.hpp>
#include <admissionspec/TokenBucket.hpp>
#include <admissionspec/TokenBucketSink.hpp>

#include <Receipt.hpp>
#include <ReputationSink.hpp>
#include <RpcSpecs.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace example {

/**
 * @brief What the admission stages may ask about one RPC client.
 */
struct RpcClient
{
    bool admin{};

    [[nodiscard]] bool
    privileged() const
    {
        return admin;
    }

    // RPC has no negotiated features or compression in this example.
    [[nodiscard]] bool
    hasFeature(admission::spec::FeatureId) const
    {
        return false;
    }

    [[nodiscard]] bool
    compressionNegotiated() const
    {
        return false;
    }
};

static_assert(admission::spec::ConnectionView<RpcClient>);

/**
 * @brief One client's requests to the RPC endpoint.
 */
class RpcHost
{
    static constexpr auto kOverLimit = admission::spec::AdmissionDecision::drop(
        admission::spec::Rule::Sender,
        "client is over its limit",
        admission::spec::PenaltyClass::None);

    RpcClient client_;
    ReputationSink reputation_;
    admission::spec::TokenBucket bucket_;

public:
    RpcHost(RpcClient client, double burst, double refillPerSecond)
        : client_{client}
        , bucket_{burst, refillPerSecond, admission::spec::TokenBucket::Clock::now()}
    {
    }

    /**
     * @brief Handle one request.
     *
     * @param path The request path, which names the method.
     * @param body The request body. Its length is what Content-Length declared.
     */
    Receipt
    handle(std::string_view path, std::string_view body)
    {
        auto bucketSink = admission::spec::TokenBucketSink{
            bucket_,
            admission::spec::TokenBucket::Clock::now(),
            admission::spec::PenaltyCosts{
                .trivial = 1.0, .useless = 5.0, .invalid = 10.0, .malformed = 20.0}};
        auto sinks = admission::spec::AllSinks{reputation_, bucketSink};

        // Stage 1: the method and Content-Length, before the body is read.
        auto const method = methodFor(path);
        if (not method)
        {
            auto const refusal = admission::spec::AdmissionDecision::drop(
                admission::spec::Rule::UnknownType,
                "unknown method",
                admission::spec::PenaltyClass::Useless);
            sinks.charge(refusal);
            return refuse("header", refusal);
        }
        auto const info = admission::spec::FrameInfo{
            .type = static_cast<uint32_t>(*method),
            .wireBytes = body.size(),
            .decodedBytes = body.size(),
            .compressed = false};

        auto const bytes =
            std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(body.data()), body.size()};
        return RpcSpecs::dispatch(
            info.type,
            [&]<class T> {
                auto const header = admission::spec::admitHeader<T>(info, client_, sinks);
                if (header.decision.dropped())
                {
                    return refuse("header", header.decision);
                }
                if (header.standing == admission::spec::Standing::Drop)
                {
                    return refuse("header", kOverLimit);
                }

                // Stage 2: walk the JSON without building a document. Containers nested deeper
                // than 8 are refused as malformed.
                auto const walked = admission::spec::admitBody<T>(
                    admission::spec::jsonVisitor(
                        bytes, admission::spec::VisitJsonOptions{.maxDepth = 8}),
                    client_,
                    sinks);
                if (walked.decision.dropped())
                {
                    return refuse("body", walked.decision);
                }

                // Stage 3: build the document, now that the walk has shown it is within limits.
                auto ec = boost::system::error_code{};
                [[maybe_unused]] auto const document = boost::json::parse(body, ec);
                if (ec)
                {
                    // The walk refuses everything the parser does, so this does not happen.
                    return Receipt{
                        .outcome = Receipt::Outcome::Refused,
                        .stage = "parse",
                        .rule = admission::spec::Rule::Malformed,
                        .reason = "did not parse"};
                }
                return Receipt{.outcome = Receipt::Outcome::Handled, .stage = "body"};
            },
            [&] {
                // methodFor names only methods the registry holds, so this does not happen.
                return refuse(
                    "header",
                    admission::spec::AdmissionDecision::drop(
                        admission::spec::Rule::UnknownType,
                        "method has no spec",
                        admission::spec::PenaltyClass::None));
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

private:
    // An RPC refusal is an error response. A client whose lasting record reaches Drop is refused
    // every request until it recovers; the transport decides whether to close.
    static Receipt
    refuse(std::string_view stage, admission::spec::AdmissionDecision const& decision)
    {
        return {
            .outcome = Receipt::Outcome::Refused,
            .stage = stage,
            .rule = decision.rule,
            .penalty = decision.penalty,
            .reason = decision.reason};
    }
};

}  // namespace example
