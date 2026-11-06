/** @file */
#pragma once

// The admission policy of every gossip message, declared in one place.
//
// Each spec says what its message may be (its size cap and repeated-field caps), what admitting
// it costs (its size ramp), who may send it, and, for Fetch, a rule only a check of its own can
// express. The generated classes are not ours to extend, so each spec is attached through
// AdmissionTraits rather than an overload in the `gossip` namespace.

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/ConnectionView.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/Rule.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/SpecRegistry.hpp>
#include <admissionspec/Tunable.hpp>
#include <admissionspec/VisitEvent.hpp>

#include <gossip.pb.h>

#include <cstdint>

namespace example {

/**
 * @brief The message type numbers the frame header carries.
 */
enum class MessageType : uint16_t { Ping = 1, Inventory = 2, AdminReport = 3, Fetch = 4 };

/**
 * @brief The protocol features a connection may negotiate.
 */
inline constexpr admission::spec::FeatureId kFetchFeature{1};

/**
 * @brief The size of an object hash.
 */
inline constexpr uint64_t kHashBytes{32};

}  // namespace example

namespace admission::spec {

// A ping is a few fixed fields. It is cheap, and charged a flat token.
template <>
struct AdmissionTraits<gossip::Ping>
{
    static consteval auto
    spec()
    {
        return makeSpec<gossip::Ping>(
                   tunable<"max_payload_bytes">(uint64_t{64}, "admission.ping.max_payload_bytes"),
                   tunable<"size_ramp">(
                       ramp({{.upToBytes = 64, .cost = 1.0}}), "admission.ping.size_ramp"))
            .withAttributes({.typeId = static_cast<uint32_t>(example::MessageType::Ping)});
    }
};

// An inventory lists up to 256 hashes, each a full hash: a shorter entry could only pad out the
// count. Larger inventories cost more to admit. Older releases forward an inventory as they
// received it, so a refusal costs only a trivial penalty: the sender may be an honest relay.
template <>
struct AdmissionTraits<gossip::Inventory>
{
    static consteval auto
    spec()
    {
        return makeSpec<gossip::Inventory>(
                   tunable<"max_payload_bytes">(
                       uint64_t{16 * 1024}, "admission.inventory.max_payload_bytes"),
                   tunable<"size_ramp">(
                       ramp(
                           {{.upToBytes = 1024, .cost = 1.0},
                            {.upToBytes = 16 * 1024, .cost = 4.0}}),
                       "admission.inventory.size_ramp"))
            .withAttributes(
                {.typeId = static_cast<uint32_t>(example::MessageType::Inventory),
                 .relayedByOlderVersions = true,
                 .repeated = repeatedCaps(
                     {{.fieldNumber = gossip::Inventory::kHashesFieldNumber,
                       .maxEntries = 256,
                       .minEntryBytes = example::kHashBytes}})});
    }
};

// Only a trusted peer may send an admin report; from anyone else it is refused unread.
template <>
struct AdmissionTraits<gossip::AdminReport>
{
    static consteval auto
    spec()
    {
        return makeSpec<gossip::AdminReport>(
                   tunable<"max_payload_bytes">(
                       uint64_t{4 * 1024}, "admission.admin_report.max_payload_bytes"),
                   tunable<"size_ramp">(
                       ramp({{.upToBytes = 4 * 1024, .cost = 1.0}}),
                       "admission.admin_report.size_ramp"))
            .withAttributes(
                {.typeId = static_cast<uint32_t>(example::MessageType::AdminReport),
                 .privilegedOnly = true});
    }
};

// A fetch needs the fetch feature, names one full hash, and asks for at most `max_limit` items. No
// cap can say "this field must be exactly 32 bytes" or "this number is at most N", so a check of
// its own does, reading the values as the payload is walked.
template <>
struct AdmissionTraits<gossip::Fetch>
{
    static consteval auto
    spec()
    {
        return makeSpec<gossip::Fetch>(
                   tunable<"max_payload_bytes">(uint64_t{128}, "admission.fetch.max_payload_bytes"),
                   tunable<"size_ramp">(
                       ramp({{.upToBytes = 128, .cost = 2.0}}), "admission.fetch.size_ramp"),
                   tunable<"max_limit">(uint64_t{100}, "admission.fetch.max_limit"))
            .withAttributes(
                {.typeId = static_cast<uint32_t>(example::MessageType::Fetch),
                 .requiresFeature = example::kFetchFeature})
            .withCheck([](VisitEvent const& event, auto const& cfg) -> AdmissionDecision {
                if (event.depth != 0)
                {
                    return AdmissionDecision::admit();
                }
                // The size is always reported, even when the stream splits the value.
                if (event.fieldNumber == gossip::Fetch::kHashFieldNumber and
                    event.size != example::kHashBytes)
                {
                    return AdmissionDecision::drop(
                        "fetch hash is not a full hash", PenaltyClass::Invalid);
                }
                if (auto const* limit = event.as<uint64_t>();
                    event.fieldNumber == gossip::Fetch::kLimitFieldNumber and limit != nullptr and
                    *limit > cfg.template get<"max_limit">())
                {
                    return AdmissionDecision::drop("fetch limit too large", PenaltyClass::Invalid);
                }
                return AdmissionDecision::admit();
            });
    }
};

}  // namespace admission::spec

namespace example {

/**
 * @brief Every gossip message type, so a frame's type number finds its spec.
 *
 * A type number two specs share does not compile.
 */
struct GossipSpecs
    : admission::spec::
          SpecRegistry<gossip::Ping, gossip::Inventory, gossip::AdminReport, gossip::Fetch>
{
};

}  // namespace example
