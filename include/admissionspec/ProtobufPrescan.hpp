/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/BaseLimits.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <admissionspec/WalkResult.hpp>
#include <admissionspec/detail/ProtobufPrescan.hpp>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

// Needs libprotobuf: build with the Conan option `with_protobuf=True` (CMake `with_protobuf=ON`).

namespace admission::spec {

/**
 * @brief Walk a protobuf payload against its schema before it is parsed, emitting @ref VisitEvent
 * s to @p check.
 *
 * It allocates nothing. It reads with libprotobuf's `CodedInputStream`, but reads tags and
 * lengths by the rules of libprotobuf's parser, which are stricter than the stream's own readers:
 * at most 5 bytes each, a tag keeping its low 32 bits, a length within int32. Unknown fields and
 * groups are skipped the same way. A differential test holds it to accepting exactly what
 * `ParsePartialFromArray` accepts.
 *
 * What @p check sees, for the fields the schema defines (unknown fields, and undefined values of
 * a closed enum, are counted against @p limits but not reported):
 * - one event per value, typed by the field's declared type as libprotobuf stores it: `Int64` for
 *   int32, int64, sint32, sint64 (zigzag-decoded), sfixed32, sfixed64 and enums; `Uint64` for
 *   uint32, uint64, fixed32 and fixed64; `Bool`; `Double` for double and float; `String` for
 *   string; `Bytes` for bytes. A string or bytes event's @c size is its length, and its @c value
 *   a view of its bytes, if the input holds them in one piece: always for a span, and for a
 *   stream when they lie in one of its buffers. Otherwise the event carries no value, so a check
 *   that reads values must allow for that;
 * - `ObjectBegin` and `ObjectEnd` around a nested message, the end's @c size its field count;
 * - every event's @c fieldNumber, and its @c depth: 0 for the top-level message's fields, one more
 *   inside each nested message.
 *
 * A repeated field's entries are reported one at a time, each with the field's number, and there
 * are no `ArrayBegin` / `ArrayEnd` events: on the wire, entries need not be contiguous, and a
 * packed field's elements are reported like unpacked entries. A spec's repeated-field caps count
 * them (see @ref RepeatedCapsCheck); a nested message's `ObjectBegin` carries its byte length as
 * @c size, so a cap's minimum entry size applies to it.
 *
 * It refuses, as @ref PenaltyClass::Malformed: an invalid tag, field number 0, wire types 6 and 7,
 * a varint over 10 bytes, and truncated fields. As @ref PenaltyClass::Invalid: more unknown fields
 * in one message, or bytes in unknown fields in the whole payload, than @p limits allow, a group
 * when @p limits rejects them, nesting deeper than @p limits allow.
 *
 * @param payload The decoded payload.
 * @param descriptor The schema of the message.
 * @param limits The limits applied to every message.
 * @param check Invoked as `AdmissionDecision(VisitEvent const&)`; the walk stops on its first
 *        drop.
 * @return Admit, or the first drop.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
prescanProtobuf(
    std::span<uint8_t const> payload,
    google::protobuf::Descriptor const& descriptor,
    BaseLimits const& limits,
    Check& check)
{
    // A payload too large for the stream is refused by the walk, which is given its true size.
    auto in = google::protobuf::io::CodedInputStream{
        payload.data(),
        static_cast<int>(std::min<size_t>(payload.size(), std::numeric_limits<int>::max()))};
    return detail::Prescan<Check>{in, payload.size(), limits, check}.run(descriptor);
}

/**
 * @brief @ref prescanProtobuf over a payload a stream delivers in pieces, such as a frame spread
 * over several receive buffers, without copying it into one.
 *
 * @param stream The stream, positioned at the payload. The walk reads exactly @p size bytes, and
 *        backs up any it read past them when it returns.
 * @param size The payload's size.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
prescanProtobuf(
    google::protobuf::io::ZeroCopyInputStream& stream,
    uint64_t size,
    google::protobuf::Descriptor const& descriptor,
    BaseLimits const& limits,
    Check& check)
{
    auto in = google::protobuf::io::CodedInputStream{&stream};
    return detail::Prescan<Check>{in, size, limits, check}.run(descriptor);
}

/**
 * @brief @ref prescanProtobuf with no check of its own.
 */
[[nodiscard]] inline AdmissionDecision
prescanProtobuf(
    std::span<uint8_t const> payload,
    google::protobuf::Descriptor const& descriptor,
    BaseLimits const& limits = {})
{
    auto admitAll = [](VisitEvent const&) { return AdmissionDecision::admit(); };
    return prescanProtobuf(payload, descriptor, limits, admitAll);
}

/**
 * @brief A walker that prescans a payload of the generated protobuf type @p T, for @ref admitBody,
 * which calls it as `walker(check)` with the spec's check and caps.
 *
 * It returns a @ref WalkResult: the decision, and the unknown fields and their bytes in the whole
 * payload, which @ref admitBody passes on in its @ref StageResult.
 *
 * @param payload The decoded payload. It must outlive the visitor.
 * @param limits The limits applied to every message.
 */
template <typename T>
    requires HasAdmissionSpec<T> and requires { T::descriptor(); }
[[nodiscard]] auto
protobufVisitor(std::span<uint8_t const> payload, BaseLimits const& limits = {})
{
    return [payload, limits]<typename Check>(Check& check) -> WalkResult {
        auto in = google::protobuf::io::CodedInputStream{
            payload.data(),
            static_cast<int>(std::min<size_t>(payload.size(), std::numeric_limits<int>::max()))};
        auto prescan = detail::Prescan<Check>{in, payload.size(), limits, check};
        auto const decision = prescan.run(*T::descriptor());
        return {.decision = decision, .totals = prescan.totals()};
    };
}

/**
 * @brief @ref protobufVisitor over a payload a stream delivers in pieces.
 *
 * @param stream The stream, positioned at the payload. It must outlive the visitor, which reads
 *        it once.
 * @param size The payload's size.
 * @param limits The limits applied to every message.
 */
template <typename T>
    requires HasAdmissionSpec<T> and requires { T::descriptor(); }
[[nodiscard]] auto
protobufVisitor(
    google::protobuf::io::ZeroCopyInputStream& stream,
    uint64_t size,
    BaseLimits const& limits = {})
{
    return [&stream, size, limits]<typename Check>(Check& check) -> WalkResult {
        auto in = google::protobuf::io::CodedInputStream{&stream};
        auto prescan = detail::Prescan<Check>{in, size, limits, check};
        auto const decision = prescan.run(*T::descriptor());
        return {.decision = decision, .totals = prescan.totals()};
    };
}

/**
 * @brief Prescan a payload of the generated protobuf type @p T, with its spec's check and caps.
 *
 * Named for its format: a JSON payload is a span of bytes too; see @ref prescanJson.
 */
template <typename T>
    requires HasAdmissionSpec<T> and requires { T::descriptor(); }
[[nodiscard]] AdmissionDecision
prescanProtobuf(std::span<uint8_t const> payload, BaseLimits const& limits = {})
{
    auto check = makeChecker<T>();
    return protobufVisitor<T>(payload, limits)(check).decision;
}

}  // namespace admission::spec
