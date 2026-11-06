/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/BaseLimits.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <admissionspec/detail/ProtobufPrescan.hpp>
#include <google/protobuf/descriptor.h>

#include <cstdint>
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
 *   string (a view of its bytes); `Bytes` for bytes. A string or bytes event's @c size is its
 *   length;
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
 * a varint over 10 bytes, and truncated fields. As @ref PenaltyClass::Invalid: more unknown fields,
 * or bytes in them, than @p limits allow, a group when @p limits rejects them, nesting deeper than
 * @p limits allow.
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
    return detail::Prescan<Check>{payload, limits, check}.run(descriptor);
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
 * @param payload The decoded payload. It must outlive the visitor.
 * @param limits The limits applied to every message.
 */
template <typename T>
    requires HasAdmissionSpec<T> and requires { T::descriptor(); }
[[nodiscard]] auto
protobufVisitor(std::span<uint8_t const> payload, BaseLimits const& limits = {})
{
    return [payload, limits]<typename Check>(Check& check) -> AdmissionDecision {
        return prescanProtobuf(payload, *T::descriptor(), limits, check);
    };
}

/**
 * @brief Prescan a payload of the generated protobuf type @p T, with its spec's check and caps.
 */
template <typename T>
    requires HasAdmissionSpec<T> and requires { T::descriptor(); }
[[nodiscard]] AdmissionDecision
prescan(std::span<uint8_t const> payload, BaseLimits const& limits = {})
{
    auto check = makeChecker<T>();
    return protobufVisitor<T>(payload, limits)(check);
}

}  // namespace admission::spec
