/** @file */
#pragma once

#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/BaseLimits.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

namespace admission::spec::detail {

/**
 * @brief One walk of a payload: the stream, the limits, and the counts kept across nested
 * messages.
 */
template <typename Check>
class Prescan
{
    std::span<uint8_t const> payload_;
    google::protobuf::io::CodedInputStream in_;
    BaseLimits const& limits_;
    RepeatedCaps const& caps_;
    Check& check_;
    uint64_t unknownFields_{};
    uint64_t unknownBytes_{};
    std::array<uint64_t, kMaxRepeatedCaps> entries_{};

public:
    Prescan(
        std::span<uint8_t const> payload,
        BaseLimits const& limits,
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
    run(google::protobuf::Descriptor const& descriptor)
    {
        if (payload_.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            return malformed("payload larger than protobuf can parse");
        }
        // A limit at the top level too, so the end of the payload reads as zero bytes left.
        auto const limit = in_.PushLimit(static_cast<int>(payload_.size()));
        auto fields = size_t{};
        auto const decision = message(descriptor, 0, fields);
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

    // Walk the fields up to the current limit, as members of `descriptor` at `depth`, counting
    // them in `fields`. The end is decided before a tag is read: a tag of 0, or one that does not
    // decode, is never an end.
    [[nodiscard]] AdmissionDecision
    message(google::protobuf::Descriptor const& descriptor, uint64_t depth, size_t& fields)
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
            ++fields;
        }
        return AdmissionDecision::admit();
    }

    [[nodiscard]] AdmissionDecision
    field(google::protobuf::Descriptor const& descriptor, uint64_t depth, uint32_t tag)
    {
        auto const number = google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag);
        auto const wireType = google::protobuf::internal::WireFormatLite::GetTagWireType(tag);
        if (number == 0)
        {
            return malformed("protobuf field number 0");
        }
        if (wireType == google::protobuf::internal::WireFormatLite::WIRETYPE_START_GROUP or
            wireType == google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP)
        {
            if (limits_.rejectGroups)
            {
                return invalid("protobuf group");
            }
        }
        else if (
            wireType != google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT and
            wireType != google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED64 and
            wireType != google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED and
            wireType != google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED32)
        {
            return malformed("invalid protobuf wire type");
        }

        auto const* known = descriptor.FindFieldByNumber(number);
        // libprotobuf keeps a field whose wire type does not match its declaration as unknown,
        // except a repeated scalar sent packed or unpacked.
        auto const declared = known != nullptr
            ? google::protobuf::internal::WireFormatLite::WireTypeForFieldType(
                  static_cast<google::protobuf::internal::WireFormatLite::FieldType>(known->type()))
            : google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT;
        bool const packed = known != nullptr and known->is_packable() and
            wireType == google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED;
        if (known == nullptr or (wireType != declared and not packed) or
            known->type() == google::protobuf::FieldDescriptor::TYPE_GROUP)
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
        switch (google::protobuf::internal::WireFormatLite::GetTagWireType(tag))
        {
            case google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT: {
                auto ignored = uint64_t{};
                return in_.ReadVarint64(&ignored) ? AdmissionDecision::admit()
                                                  : malformed("invalid protobuf varint");
            }
            case google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED64:
                return in_.Skip(8) ? AdmissionDecision::admit()
                                   : malformed("truncated protobuf field");
            case google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED32:
                return in_.Skip(4) ? AdmissionDecision::admit()
                                   : malformed("truncated protobuf field");
            case google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
                auto length = uint32_t{};
                return readLength(length) and in_.Skip(static_cast<int>(length))
                    ? AdmissionDecision::admit()
                    : malformed("truncated protobuf field");
            }
            case google::protobuf::internal::WireFormatLite::WIRETYPE_START_GROUP:
                return group(
                    google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag), depth + 1);
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
            if (not readTag(tag) or
                google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag) == 0)
            {
                return malformed("invalid protobuf tag");
            }
            if (google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
                google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP)
            {
                return google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag) == number
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
    entry(google::protobuf::FieldDescriptor const& field, uint64_t depth, uint64_t bytes)
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

    // Report a scalar of `field` at `depth`, as the event its C++ type maps to.
    template <typename V>
    [[nodiscard]] AdmissionDecision
    scalar(google::protobuf::FieldDescriptor const& field, uint64_t depth, V v)
    {
        auto const kind = [] {
            if constexpr (std::is_same_v<V, bool>)
            {
                return EventKind::Bool;
            }
            else if constexpr (std::is_same_v<V, double>)
            {
                return EventKind::Double;
            }
            else if constexpr (std::is_same_v<V, int64_t>)
            {
                return EventKind::Int64;
            }
            else
            {
                static_assert(std::is_same_v<V, uint64_t>);
                return EventKind::Uint64;
            }
        }();
        return emit(
            {.kind = kind,
             .depth = static_cast<uint32_t>(depth),
             .fieldNumber = static_cast<uint64_t>(field.number()),
             .value = v});
    }

    // Read one value of a known field with its declared wire type, and report it typed by the
    // field's declared type, as libprotobuf would store it.
    [[nodiscard]] AdmissionDecision
    value(
        google::protobuf::FieldDescriptor const& field,
        google::protobuf::internal::WireFormatLite::WireType wireType,
        uint64_t depth)
    {
        switch (wireType)
        {
            case google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT: {
                auto v = uint64_t{};
                if (not in_.ReadVarint64(&v))
                {
                    return malformed("invalid protobuf varint");
                }
                switch (field.type())
                {
                    case google::protobuf::FieldDescriptor::TYPE_INT32:
                        return scalar(field, depth, static_cast<int64_t>(static_cast<int32_t>(v)));
                    case google::protobuf::FieldDescriptor::TYPE_INT64:
                        return scalar(field, depth, static_cast<int64_t>(v));
                    case google::protobuf::FieldDescriptor::TYPE_UINT32:
                        return scalar(
                            field, depth, static_cast<uint64_t>(static_cast<uint32_t>(v)));
                    case google::protobuf::FieldDescriptor::TYPE_SINT32:
                        return scalar(
                            field,
                            depth,
                            static_cast<int64_t>(
                                google::protobuf::internal::WireFormatLite::ZigZagDecode32(
                                    static_cast<uint32_t>(v))));
                    case google::protobuf::FieldDescriptor::TYPE_SINT64:
                        return scalar(
                            field,
                            depth,
                            static_cast<int64_t>(
                                google::protobuf::internal::WireFormatLite::ZigZagDecode64(v)));
                    case google::protobuf::FieldDescriptor::TYPE_BOOL:
                        return scalar(field, depth, v != 0);
                    case google::protobuf::FieldDescriptor::TYPE_ENUM:
                        return enumValue(field, depth, static_cast<int32_t>(v));
                    default:
                        return scalar(field, depth, v);
                }
            }
            case google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED64: {
                auto v = uint64_t{};
                if (not in_.ReadLittleEndian64(&v))
                {
                    return malformed("truncated protobuf field");
                }
                switch (field.type())
                {
                    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
                        return scalar(field, depth, std::bit_cast<int64_t>(v));
                    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
                        return scalar(field, depth, std::bit_cast<double>(v));
                    default:
                        return scalar(field, depth, v);
                }
            }
            case google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED32: {
                auto v = uint32_t{};
                if (not in_.ReadLittleEndian32(&v))
                {
                    return malformed("truncated protobuf field");
                }
                switch (field.type())
                {
                    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
                        return scalar(
                            field, depth, static_cast<int64_t>(std::bit_cast<int32_t>(v)));
                    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
                        return scalar(field, depth, static_cast<double>(std::bit_cast<float>(v)));
                    default:
                        return scalar(field, depth, static_cast<uint64_t>(v));
                }
            }
            case google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED:
                return lengthDelimited(field, depth);
            default:
                return malformed("invalid protobuf wire type");
        }
    }

    // An enum value. libprotobuf keeps an undefined value of a closed enum as an unknown field, so
    // it counts against the unknown-field limits and is not reported.
    [[nodiscard]] AdmissionDecision
    enumValue(google::protobuf::FieldDescriptor const& field, uint64_t depth, int32_t v)
    {
        if (field.enum_type()->is_closed() and field.enum_type()->FindValueByNumber(v) == nullptr)
        {
            unknownBytes_ += 1;
            if (++unknownFields_ > limits_.maxUnknownFields)
            {
                return invalid("too many unknown protobuf fields");
            }
            return AdmissionDecision::admit();
        }
        return scalar(field, depth, static_cast<int64_t>(v));
    }

    [[nodiscard]] AdmissionDecision
    lengthDelimited(google::protobuf::FieldDescriptor const& field, uint64_t depth)
    {
        auto length = uint32_t{};
        if (not readLength(length))
        {
            return malformed("truncated protobuf field");
        }
        auto const number = static_cast<uint64_t>(field.number());
        auto const at = static_cast<uint32_t>(depth);

        if (field.type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE)
        {
            if (depth + 1 > limits_.maxDepth)
            {
                return invalid("protobuf messages nested too deeply");
            }
            if (auto const d =
                    emit({.kind = EventKind::ObjectBegin, .depth = at, .fieldNumber = number});
                d.dropped())
            {
                return d;
            }
            auto const limit = in_.PushLimit(static_cast<int>(length));
            auto fields = size_t{};
            auto const decision = message(*field.message_type(), depth + 1, fields);
            in_.PopLimit(limit);
            if (decision.dropped())
            {
                return decision;
            }
            return emit(
                {.kind = EventKind::ObjectEnd, .size = fields, .depth = at, .fieldNumber = number});
        }

        // A string or bytes field: report it, then step over it.
        auto const offset = static_cast<size_t>(in_.CurrentPosition());
        auto const body = payload_.subspan(offset, length);
        if (not in_.Skip(static_cast<int>(length)))
        {
            return malformed("truncated protobuf field");
        }
        if (field.type() == google::protobuf::FieldDescriptor::TYPE_STRING)
        {
            return emit(
                {.kind = EventKind::String,
                 .size = body.size(),
                 .depth = at,
                 .fieldNumber = number,
                 .value =
                     std::string_view{reinterpret_cast<char const*>(body.data()), body.size()}});
        }
        return emit(
            {.kind = EventKind::Bytes,
             .size = body.size(),
             .depth = at,
             .fieldNumber = number,
             .value = body});
    }

    // A packed repeated scalar: each element is an entry.
    [[nodiscard]] AdmissionDecision
    packedEntries(google::protobuf::FieldDescriptor const& field, uint64_t depth)
    {
        auto length = uint32_t{};
        if (not readLength(length))
        {
            return malformed("truncated protobuf field");
        }
        auto const element = google::protobuf::internal::WireFormatLite::WireTypeForFieldType(
            static_cast<google::protobuf::internal::WireFormatLite::FieldType>(field.type()));
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

}  // namespace admission::spec::detail
