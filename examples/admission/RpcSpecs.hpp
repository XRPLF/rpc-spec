/** @file */
#pragma once

// The admission policy of every RPC method, declared in one place. The JSON counterpart of
// GossipSpecs.hpp: the same spec type, stages and sinks, with a JSON walker in place of the
// protobuf prescan.
//
// A request type here is only a name for the method's spec, since the request is admitted before
// any object is built from it. These types are ours, so each spec is an overload found by
// argument-dependent lookup, the route for types you own.

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/SpecRegistry.hpp>
#include <admissionspec/Tunable.hpp>
#include <admissionspec/VisitEvent.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace example {

/**
 * @brief The method ids the endpoint maps request paths to. They play the part a message type
 * number plays for a peer frame.
 */
enum class Method : uint32_t { ServerInfo = 1, Submit = 2, Stop = 3, History = 4 };

/**
 * @brief The method a request path names, if any: `/v1/<method>`.
 *
 * The method is known before the body is read, so the header stage can apply its spec. A JSON-RPC
 * request that names its method inside the body needs a first, small pass over the body to find
 * it; everything after that is the same.
 */
[[nodiscard]] constexpr std::optional<Method>
methodFor(std::string_view path)
{
    constexpr auto kMethods = std::array{
        std::pair{std::string_view{"/v1/server_info"}, Method::ServerInfo},
        std::pair{std::string_view{"/v1/submit"}, Method::Submit},
        std::pair{std::string_view{"/v1/stop"}, Method::Stop},
        std::pair{std::string_view{"/v1/history"}, Method::History},
    };
    for (auto const& [name, method] : kMethods)
    {
        if (name == path)
        {
            return method;
        }
    }
    return std::nullopt;
}

struct ServerInfoRequest
{
};

struct SubmitRequest
{
};

struct StopRequest
{
};

struct HistoryRequest
{
};

// Takes no parameters: a small body only.
consteval auto
admissionSpec(std::type_identity<ServerInfoRequest>)
{
    return admission::spec::makeSpec<ServerInfoRequest>(
               admission::spec::tunable<"max_payload_bytes">(
                   uint64_t{256}, "admission.rpc.server_info.max_payload_bytes"),
               admission::spec::tunable<"size_ramp">(
                   admission::spec::ramp({{.upToBytes = 256, .cost = 1.0}}),
                   "admission.rpc.server_info.size_ramp"))
        .withAttributes({.typeId = static_cast<uint32_t>(Method::ServerInfo)});
}

// Submits up to 8 signed blobs, as hex strings, each at least a minimal transaction's length. The
// cap names the array by its key, where a protobuf cap names a field number.
consteval auto
admissionSpec(std::type_identity<SubmitRequest>)
{
    return admission::spec::makeSpec<SubmitRequest>(
               admission::spec::tunable<"max_payload_bytes">(
                   uint64_t{64 * 1024}, "admission.rpc.submit.max_payload_bytes"),
               admission::spec::tunable<"size_ramp">(
                   admission::spec::ramp(
                       {{.upToBytes = 4 * 1024, .cost = 2.0},
                        {.upToBytes = 64 * 1024, .cost = 10.0}}),
                   "admission.rpc.submit.size_ramp"))
        .withAttributes(
            {.typeId = static_cast<uint32_t>(Method::Submit),
             .repeated = admission::spec::repeatedCaps(
                 {{.key = "tx_blobs", .maxEntries = 8, .minEntryBytes = 64}})});
}

// Only a client with the admin role may stop the server.
consteval auto
admissionSpec(std::type_identity<StopRequest>)
{
    return admission::spec::makeSpec<StopRequest>(
               admission::spec::tunable<"max_payload_bytes">(
                   uint64_t{256}, "admission.rpc.stop.max_payload_bytes"),
               admission::spec::tunable<"size_ramp">(
                   admission::spec::ramp({{.upToBytes = 256, .cost = 1.0}}),
                   "admission.rpc.stop.size_ramp"))
        .withAttributes({.typeId = static_cast<uint32_t>(Method::Stop), .privilegedOnly = true});
}

// Pages through an account's history, at most `max_limit` entries a page. As for a fetch, no cap
// can bound a number, so a check of its own reads the value as the body is walked, and a request
// over the limit is refused before the body is parsed.
consteval auto
admissionSpec(std::type_identity<HistoryRequest>)
{
    return admission::spec::makeSpec<HistoryRequest>(
               admission::spec::tunable<"max_payload_bytes">(
                   uint64_t{1024}, "admission.rpc.history.max_payload_bytes"),
               admission::spec::tunable<"size_ramp">(
                   admission::spec::ramp({{.upToBytes = 1024, .cost = 3.0}}),
                   "admission.rpc.history.size_ramp"),
               admission::spec::tunable<"max_limit">(
                   int64_t{200}, "admission.rpc.history.max_limit"))
        .withAttributes({.typeId = static_cast<uint32_t>(Method::History)})
        .withCheck(
            [](admission::spec::VisitEvent const& event,
               auto const& cfg) -> admission::spec::AdmissionDecision {
                // A top-level member is at depth 1, inside the request object.
                if (auto const* limit = event.as<int64_t>(); event.depth == 1 and
                    event.key == "limit" and limit != nullptr and
                    (*limit < 1 or *limit > cfg.template get<"max_limit">()))
                {
                    return admission::spec::AdmissionDecision::drop(
                        "history limit out of range", admission::spec::PenaltyClass::Invalid);
                }
                return admission::spec::AdmissionDecision::admit();
            });
}

/**
 * @brief Every RPC method, so a method id finds its spec.
 */
struct RpcSpecs
    : admission::spec::SpecRegistry<ServerInfoRequest, SubmitRequest, StopRequest, HistoryRequest>
{
};

}  // namespace example
