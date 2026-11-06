/** @file */
#pragma once

#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/SpecAttributes.hpp>
#include <admissionspec/Types.hpp>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

// Needs libprotobuf: build with the Conan option `with_protobuf=True` (CMake `with_protobuf=ON`).

namespace admission::spec {

/**
 * @brief The resolved values of @ref BaseLimits that the prescan enforces.
 */
struct PrescanLimits
{
    uint64_t maxUnknownFields{16};
    uint64_t maxUnknownFieldBytes{1024};
    uint64_t maxDepth{100};
    bool rejectGroups{true};
};

/**
 * @brief The prescan limits from @ref BaseLimits' defaults.
 */
[[nodiscard]] constexpr PrescanLimits
prescanLimits(BaseLimits const& base = {}) noexcept
{
    return {
        .maxUnknownFields = base.maxUnknownFields.defaultValue,
        .maxUnknownFieldBytes = base.maxUnknownFieldBytes.defaultValue,
        .maxDepth = base.maxDepth.defaultValue,
        .rejectGroups = base.rejectGroups.defaultValue};
}

namespace detail {

namespace gpb = google::protobuf;
using WireFormat = gpb::internal::WireFormatLite;

/**
 * @brief One walk of a payload: the stream, the limits, and the counts kept across nested
 * messages.
 */
template <typename Check>
class Prescan
{
    std::span<uint8_t const> payload_;
    gpb::io::CodedInputStream in_;
    PrescanLimits const& limits_;
    RepeatedCaps const& caps_;
    Check& check_;
    uint64_t unknownFields_{};
    uint64_t unknownBytes_{};
    std::array<uint64_t, kMaxRepeatedCaps> entries_{};

public:
    Prescan(
        std::span<uint8_t const> payload,
        PrescanLimits const& limits,
        RepeatedCaps const& caps,
        Check& check)
        : payload_{payload}
        , in_{payload.data(), static_cast<int>(payload.size())}
        , limits_{limits}
        , caps_{caps}
        , check_{check}
    {
    }

    [[nodiscard]] AdmissionDecision
    run(gpb::Descriptor const& descriptor)
    {
        if (payload_.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            return malformed("payload larger than protobuf can parse");
        }
        // A limit at the top level too, so the end of the payload reads as zero bytes left.
        auto const limit = in_.PushLimit(static_cast<int>(payload_.size()));
        auto const decision = message(descriptor, 0);
        in_.PopLimit(limit);
        return decision;
    }

private:
    [[nodiscard]] static AdmissionDecision
    malformed(std::string_view why)
    {
        return AdmissionDecision::drop(why, PenaltyClass::Malformed);
    }

    [[nodiscard]] static AdmissionDecision
    invalid(std::string_view why)
    {
        return AdmissionDecision::drop(why, PenaltyClass::Invalid);
    }

    // libprotobuf's parser takes a tag or a length in at most 5 bytes. A tag keeps its low 32
    // bits; a length must fit int32. CodedInputStream's own readers accept longer encodings, so
    // both are read as plain varints and checked here.
    static constexpr int kMaxTagOrLengthBytes = 5;

    [[nodiscard]] bool
    readBounded(uint64_t& value, uint64_t max)
    {
        auto const start = in_.CurrentPosition();
        return in_.ReadVarint64(&value) and
            in_.CurrentPosition() - start <= kMaxTagOrLengthBytes and value <= max;
    }

    [[nodiscard]] bool
    readTag(uint32_t& tag)
    {
        auto value = uint64_t{};
        if (not readBounded(value, std::numeric_limits<uint64_t>::max()))
        {
            return false;
        }
        tag = static_cast<uint32_t>(value);
        return tag != 0;
    }

    [[nodiscard]] bool
    readLength(uint32_t& length)
    {
        auto value = uint64_t{};
        if (not readBounded(value, static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) or
            static_cast<int64_t>(value) > static_cast<int64_t>(in_.BytesUntilLimit()))
        {
            return false;
        }
        length = static_cast<uint32_t>(value);
        return true;
    }

    // Walk the fields up to the current limit, as members of `descriptor`. The end is decided
    // before a tag is read: a tag of 0, or one that does not decode, is never an end.
    [[nodiscard]] AdmissionDecision
    message(gpb::Descriptor const& descriptor, uint64_t depth)
    {
        while (in_.BytesUntilLimit() > 0)
        {
            auto tag = uint32_t{};
            if (not readTag(tag))
            {
                return malformed("invalid protobuf tag");
            }
            if (auto const decision = field(descriptor, depth, tag); decision.dropped())
            {
                return decision;
            }
        }
        return AdmissionDecision::admit();
    }

    [[nodiscard]] AdmissionDecision
    field(gpb::Descriptor const& descriptor, uint64_t depth, uint32_t tag)
    {
        auto const number = WireFormat::GetTagFieldNumber(tag);
        auto const wireType = WireFormat::GetTagWireType(tag);
        if (number == 0)
        {
            return malformed("protobuf field number 0");
        }
        if (wireType == WireFormat::WIRETYPE_START_GROUP or
            wireType == WireFormat::WIRETYPE_END_GROUP)
        {
            if (limits_.rejectGroups)
            {
                return invalid("protobuf group");
            }
        }
        else if (
            wireType != WireFormat::WIRETYPE_VARINT and wireType != WireFormat::WIRETYPE_FIXED64 and
            wireType != WireFormat::WIRETYPE_LENGTH_DELIMITED and
            wireType != WireFormat::WIRETYPE_FIXED32)
        {
            return malformed("invalid protobuf wire type");
        }

        auto const* known = descriptor.FindFieldByNumber(number);
        // libprotobuf keeps a field whose wire type does not match its declaration as unknown,
        // except a repeated scalar sent packed or unpacked.
        auto const declared = known != nullptr
            ? WireFormat::WireTypeForFieldType(static_cast<WireFormat::FieldType>(known->type()))
            : WireFormat::WIRETYPE_VARINT;
        bool const packed = known != nullptr and known->is_packable() and
            wireType == WireFormat::WIRETYPE_LENGTH_DELIMITED;
        if (known == nullptr or (wireType != declared and not packed) or
            known->type() == gpb::FieldDescriptor::TYPE_GROUP)
        {
            return unknown(tag, depth);
        }

        if (packed)
        {
            return packedEntries(*known, depth);
        }
        auto const start = in_.CurrentPosition();
        auto decision = value(*known, wireType, depth);
        if (decision.dropped())
        {
            return decision;
        }
        return entry(*known, depth, static_cast<uint64_t>(in_.CurrentPosition() - start));
    }

    // Skip a field the schema does not define, counting it against the unknown-field limits.
    [[nodiscard]] AdmissionDecision
    unknown(uint32_t tag, uint64_t depth)
    {
        auto const start = in_.CurrentPosition();
        if (auto const decision = skip(tag, depth); decision.dropped())
        {
            return decision;
        }
        unknownBytes_ += static_cast<uint64_t>(in_.CurrentPosition() - start);
        if (++unknownFields_ > limits_.maxUnknownFields)
        {
            return invalid("too many unknown protobuf fields");
        }
        if (unknownBytes_ > limits_.maxUnknownFieldBytes)
        {
            return invalid("too many bytes in unknown protobuf fields");
        }
        return AdmissionDecision::admit();
    }

    // Step over one value of wire type `tag`'s, a group to its matching end.
    [[nodiscard]] AdmissionDecision
    skip(uint32_t tag, uint64_t depth)
    {
        switch (WireFormat::GetTagWireType(tag))
        {
            case WireFormat::WIRETYPE_VARINT: {
                auto ignored = uint64_t{};
                return in_.ReadVarint64(&ignored) ? AdmissionDecision::admit()
                                                  : malformed("invalid protobuf varint");
            }
            case WireFormat::WIRETYPE_FIXED64:
                return in_.Skip(8) ? AdmissionDecision::admit()
                                   : malformed("truncated protobuf field");
            case WireFormat::WIRETYPE_FIXED32:
                return in_.Skip(4) ? AdmissionDecision::admit()
                                   : malformed("truncated protobuf field");
            case WireFormat::WIRETYPE_LENGTH_DELIMITED: {
                auto length = uint32_t{};
                return readLength(length) and in_.Skip(static_cast<int>(length))
                    ? AdmissionDecision::admit()
                    : malformed("truncated protobuf field");
            }
            case WireFormat::WIRETYPE_START_GROUP:
                return group(WireFormat::GetTagFieldNumber(tag), depth + 1);
            default:
                // An end-group tag outside its group.
                return malformed("unmatched protobuf end group");
        }
    }

    // Step over a group's fields, up to the end-group tag of the same field number.
    [[nodiscard]] AdmissionDecision
    group(int number, uint64_t depth)
    {
        if (depth > limits_.maxDepth)
        {
            return invalid("protobuf messages nested too deeply");
        }
        while (in_.BytesUntilLimit() > 0)
        {
            auto tag = uint32_t{};
            if (not readTag(tag) or WireFormat::GetTagFieldNumber(tag) == 0)
            {
                return malformed("invalid protobuf tag");
            }
            if (WireFormat::GetTagWireType(tag) == WireFormat::WIRETYPE_END_GROUP)
            {
                return WireFormat::GetTagFieldNumber(tag) == number
                    ? AdmissionDecision::admit()
                    : malformed("unmatched protobuf end group");
            }
            if (auto const decision = skip(tag, depth); decision.dropped())
            {
                return decision;
            }
        }
        return malformed("unterminated protobuf group");
    }

    // Count one entry of a repeated field, at the top level, against its cap.
    [[nodiscard]] AdmissionDecision
    entry(gpb::FieldDescriptor const& field, uint64_t depth, uint64_t bytes)
    {
        if (depth != 0 or not field.is_repeated())
        {
            return AdmissionDecision::admit();
        }
        for (size_t i = 0; i < caps_.count; ++i)
        {
            if (caps_.caps[i].fieldNumber == static_cast<uint32_t>(field.number()))
            {
                if (++entries_[i] > caps_.caps[i].maxEntries)
                {
                    return invalid("too many entries in a repeated protobuf field");
                }
                if (bytes < caps_.caps[i].minEntryBytes)
                {
                    return invalid("repeated protobuf entry smaller than its type allows");
                }
            }
        }
        return AdmissionDecision::admit();
    }

    [[nodiscard]] AdmissionDecision
    emit(VisitEvent const& event)
    {
        return check_(event);
    }

    // Read one value of a known field with its declared wire type.
    [[nodiscard]] AdmissionDecision
    value(gpb::FieldDescriptor const& field, WireFormat::WireType wireType, uint64_t depth)
    {
        auto const number = static_cast<uint64_t>(field.number());
        switch (wireType)
        {
            case WireFormat::WIRETYPE_VARINT: {
                auto v = uint64_t{};
                if (not in_.ReadVarint64(&v))
                {
                    return malformed("invalid protobuf varint");
                }
                if (field.type() == gpb::FieldDescriptor::TYPE_ENUM and
                    field.enum_type()->is_closed() and
                    field.enum_type()->FindValueByNumber(static_cast<int>(v)) == nullptr)
                {
                    // libprotobuf keeps an undefined value of a closed enum as an unknown field.
                    unknownBytes_ += 1;
                    if (++unknownFields_ > limits_.maxUnknownFields)
                    {
                        return invalid("too many unknown protobuf fields");
                    }
                }
                return emit({.kind = EventKind::Uint64, .fieldNumber = number, .value = v});
            }
            case WireFormat::WIRETYPE_FIXED64: {
                auto v = uint64_t{};
                if (not in_.ReadLittleEndian64(&v))
                {
                    return malformed("truncated protobuf field");
                }
                return emit({.kind = EventKind::Uint64, .fieldNumber = number, .value = v});
            }
            case WireFormat::WIRETYPE_FIXED32: {
                auto v = uint32_t{};
                if (not in_.ReadLittleEndian32(&v))
                {
                    return malformed("truncated protobuf field");
                }
                return emit(
                    {.kind = EventKind::Uint64,
                     .fieldNumber = number,
                     .value = static_cast<uint64_t>(v)});
            }
            case WireFormat::WIRETYPE_LENGTH_DELIMITED:
                return lengthDelimited(field, depth);
            default:
                return malformed("invalid protobuf wire type");
        }
    }

    [[nodiscard]] AdmissionDecision
    lengthDelimited(gpb::FieldDescriptor const& field, uint64_t depth)
    {
        auto length = uint32_t{};
        if (not readLength(length))
        {
            return malformed("truncated protobuf field");
        }
        auto const number = static_cast<uint64_t>(field.number());

        if (field.type() == gpb::FieldDescriptor::TYPE_MESSAGE)
        {
            if (depth + 1 > limits_.maxDepth)
            {
                return invalid("protobuf messages nested too deeply");
            }
            if (auto const d = emit({.kind = EventKind::ObjectBegin, .fieldNumber = number});
                d.dropped())
            {
                return d;
            }
            auto const limit = in_.PushLimit(static_cast<int>(length));
            auto const decision = message(*field.message_type(), depth + 1);
            in_.PopLimit(limit);
            if (decision.dropped())
            {
                return decision;
            }
            return emit({.kind = EventKind::ObjectEnd, .fieldNumber = number});
        }

        // A string or bytes field: report the span, then step over it.
        auto const offset = static_cast<size_t>(in_.CurrentPosition());
        auto const body = payload_.subspan(offset, length);
        if (not in_.Skip(static_cast<int>(length)))
        {
            return malformed("truncated protobuf field");
        }
        return emit({.kind = EventKind::Bytes, .fieldNumber = number, .value = body});
    }

    // A packed repeated scalar: each element is an entry.
    [[nodiscard]] AdmissionDecision
    packedEntries(gpb::FieldDescriptor const& field, uint64_t depth)
    {
        auto length = uint32_t{};
        if (not readLength(length))
        {
            return malformed("truncated protobuf field");
        }
        auto const element =
            WireFormat::WireTypeForFieldType(static_cast<WireFormat::FieldType>(field.type()));
        auto const limit = in_.PushLimit(static_cast<int>(length));
        auto decision = AdmissionDecision::admit();
        while (in_.BytesUntilLimit() > 0 and decision.admitted())
        {
            auto const start = in_.CurrentPosition();
            decision = value(field, element, depth);
            if (decision.admitted())
            {
                decision =
                    entry(field, depth, static_cast<uint64_t>(in_.CurrentPosition() - start));
            }
        }
        in_.PopLimit(limit);
        return decision;
    }
};

}  // namespace detail

/**
 * @brief Walk a protobuf payload against its schema before it is parsed, emitting @ref VisitEvent
 * s to @p check.
 *
 * It allocates nothing. It reads with libprotobuf's `CodedInputStream`, but reads tags and
 * lengths by the rules of libprotobuf's parser, which are stricter than the stream's own readers:
 * at most 5 bytes each, a tag keeping its low 32 bits, a length within int32. Unknown fields and
 * groups are skipped the same way. A differential test holds it to accepting exactly what
 * `ParsePartialFromArray` accepts. It refuses, as
 * @ref PenaltyClass::Malformed: an invalid tag, field number 0, wire types 6 and 7, a varint over
 * 10 bytes, and truncated fields. As @ref PenaltyClass::Invalid: more unknown fields, or bytes in
 * them, than @p limits allow (an undefined value of a closed enum counts as one), a group when
 * @p limits rejects them, nesting deeper than @p limits allow, and a top-level repeated field over
 * its cap in @p caps or with an entry smaller than the cap allows.
 *
 * @param payload The decoded payload.
 * @param descriptor The schema of the message.
 * @param limits The limits applied to every message.
 * @param caps The message's repeated-field caps.
 * @param check Invoked as `AdmissionDecision(VisitEvent const&)` for each value and each nested
 *        message's begin and end; the walk stops on its first drop.
 * @return Admit, or the first drop.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
prescanProtobuf(
    std::span<uint8_t const> payload,
    google::protobuf::Descriptor const& descriptor,
    PrescanLimits const& limits,
    RepeatedCaps const& caps,
    Check& check)
{
    return detail::Prescan<Check>{payload, limits, caps, check}.run(descriptor);
}

/**
 * @brief @ref prescanProtobuf with no check of its own.
 */
[[nodiscard]] inline AdmissionDecision
prescanProtobuf(
    std::span<uint8_t const> payload,
    google::protobuf::Descriptor const& descriptor,
    PrescanLimits const& limits = prescanLimits(),
    RepeatedCaps const& caps = {})
{
    auto admitAll = [](VisitEvent const&) { return AdmissionDecision::admit(); };
    return prescanProtobuf(payload, descriptor, limits, caps, admitAll);
}

/**
 * @brief Prescan a payload of the generated protobuf type @p T, with its spec's repeated caps
 * and streaming check.
 */
template <typename T>
    requires HasAdmissionSpec<T> and requires { T::descriptor(); }
[[nodiscard]] AdmissionDecision
prescan(std::span<uint8_t const> payload, PrescanLimits const& limits = prescanLimits())
{
    static constexpr auto kSpec = admissionSpecFor<T>();
    auto check = makeChecker<T>();
    return prescanProtobuf(payload, *T::descriptor(), limits, kSpec.attributes().repeated, check);
}

}  // namespace admission::spec
