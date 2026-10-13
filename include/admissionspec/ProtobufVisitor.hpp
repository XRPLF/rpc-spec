/** @file */
#pragma once

#include <admissionspec/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace admission::spec {

namespace detail {

template <typename T>
struct PackedTrait;

/**
 * @brief Element traits for a packed protobuf field: how to read it and how to interpret it.
 */
template <>
struct PackedTrait<uint32_t>
{
    /**
     * @brief The integer type the element is read as.
     */
    using ReadType = uint32_t;

    /**
     * @brief The integer type the element's value is interpreted as.
     */
    using WriteType = int32_t;
};

/**
 * @brief Element traits for a packed protobuf field: how to read it and how to interpret it.
 */
template <>
struct PackedTrait<int32_t>
{
    /**
     * @brief The integer type the element is read as.
     */
    using ReadType = uint32_t;

    /**
     * @brief The integer type the element's value is interpreted as.
     */
    using WriteType = int32_t;
};

/**
 * @brief Element traits for a packed protobuf field: how to read it and how to interpret it.
 */
template <>
struct PackedTrait<uint64_t>
{
    /**
     * @brief The integer type the element is read as.
     */
    using ReadType = uint64_t;

    /**
     * @brief The integer type the element's value is interpreted as.
     */
    using WriteType = int64_t;
};

/**
 * @brief Element traits for a packed protobuf field: how to read it and how to interpret it.
 */
template <>
struct PackedTrait<int64_t>
{
    /**
     * @brief The integer type the element is read as.
     */
    using ReadType = uint64_t;

    /**
     * @brief The integer type the element's value is interpreted as.
     */
    using WriteType = int64_t;
};

enum class WireType {
    Varint = 0,  ///< Variable int32_t, int64_t, uint32_t, uint64_t, bool, enum
    I64 = 1,     ///< int64_t, uint64_t, double
    Len = 2,     ///< string, bytes, sub messages, repeated fields
    I32 = 5,     ///< int32_t, uint32_t, float
};

/**
 * @brief Read a base-128 varint from @p bytes at @p pos, advancing it.
 *
 * @param bytes The buffer to read from.
 * @param pos The offset of the varint; advanced past it.
 * @param out Receives the decoded value on success.
 * @return False if the varint is truncated.
 */
[[nodiscard]] inline bool
readVarint(std::span<uint8_t const> bytes, size_t& pos, uint64_t& out)
{
    auto result = uint64_t{};
    auto shift = uint64_t{};
    while (pos < bytes.size())
    {
        auto const byte = bytes[pos++];
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
        {
            out = result;
            return true;
        }
        shift += 7;
    }
    return false;
}

}  // namespace detail

/**
 * @brief Run-time options for @ref visitProtobuf and @ref visitPackedVarint.
 */
struct ProtobufVisitorOptions
{
    /**
     * @brief Token cost of the drop returned for a payload that is not a valid protobuf encoding.
     */
    double costForInvalidPayload{10};
};

/**
 * @brief Visit a serialized protobuf message, emitting one @ref VisitEvent per field into @p check.
 *
 * Scalars (varint / 32- / 64-bit) are reported as @c Int64 with their value. A length-delimited
 * field is ambiguous on the wire — string, packed list, or sub-message — so the visitor does not
 * guess: it reports it as @c Bytes, with @c value a span over exactly that field's bytes. The spec
 * author, who has the schema, decides what to do with it: read it as a scalar, or re-enter over the
 * span with `visitProtobuf` (sub-message), `visitPackedVarint`, or `visitPackedFixed` (packed
 * list). Stops and returns on the first drop.
 *
 * @tparam Check The per-message check, invoked as `AdmissionDecision(VisitEvent const&)`.
 * @param bytes The serialized message.
 * @param check The per-message check to invoke.
 * @param options The cost of a malformed payload.
 * @return The check's drop, a drop for a malformed payload, or admit.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
visitProtobuf(
    std::span<uint8_t const> bytes,
    Check& check,
    ProtobufVisitorOptions const& options = {})
{
    auto pos = size_t{};
    while (pos < bytes.size())
    {
        auto tag = uint64_t{};
        if (not detail::readVarint(bytes, pos, tag))
        {
            return AdmissionDecision::drop(
                "Invalid protobuf payload", options.costForInvalidPayload);
        }
        auto const field = static_cast<uint64_t>(tag >> 3);
        auto const wireType = static_cast<uint64_t>(tag & 0x07);

        auto scalar = [&](int64_t scalarValue) {
            return check(
                VisitEvent{.kind = EventKind::Int64, .fieldNumber = field, .value = scalarValue});
        };

        auto handleInt = [&](auto size) {
            if (pos + size > bytes.size())
            {
                return AdmissionDecision::drop(
                    "Invalid protobuf payload", options.costForInvalidPayload);
            }
            auto value = uint64_t{};
            std::memcpy(&value, &bytes[pos], size);
            pos += size;
            return scalar(static_cast<int64_t>(value));
        };

        switch (static_cast<detail::WireType>(wireType))
        {
            using enum detail::WireType;
            case Varint: {
                auto value = uint64_t{};
                if (not detail::readVarint(bytes, pos, value))
                {
                    return AdmissionDecision::drop(
                        "Invalid protobuf payload", options.costForInvalidPayload);
                }
                if (auto const decision = scalar(static_cast<int64_t>(value)); decision.dropped())
                {
                    return decision;
                }
            }
            break;
            case I64: {
                if (auto const decision = handleInt(8); decision.dropped())
                {
                    return decision;
                }
            }
            break;
            case I32: {
                if (auto const decision = handleInt(4); decision.dropped())
                {
                    return decision;
                }
            }
            break;
            case Len: {
                auto len = uint64_t{};
                if (not detail::readVarint(bytes, pos, len) or pos + len > bytes.size())
                {
                    return AdmissionDecision::drop(
                        "Invalid protobuf payload", options.costForInvalidPayload);
                }
                auto const body = bytes.subspan(pos, static_cast<size_t>(len));
                pos += static_cast<size_t>(len);

                // Report the raw span. Its meaning (string / packed list / sub-message) is schema,
                // so the author decides: read it as a scalar, or re-enter with visitProtobuf /
                // visitPackedVarint / visitPackedFixed over these bytes.
                if (auto const decision = check(
                        VisitEvent{.kind = EventKind::Bytes, .fieldNumber = field, .value = body});
                    decision.dropped())
                {
                    return decision;
                }
            }
            break;
        }
    }
    return AdmissionDecision::admit();
}

/**
 * @brief Re-enter over a packed @c repeated payload, emitting each element as an @c Int64 of @p
 * field.
 *
 * A packed field's bytes are the element values concatenated with no tags, so the message visitor
 * cannot parse them — but the element wire type is known from the schema, so the spec author picks
 * the matching visitor. This is a flat scan of one level (not a recursive descent): each element is
 * a sibling, so to the check the stream is identical to an *unpacked* repeated field. Use for
 * packed varint types (int32/64, uint32/64, bool, enum, sint via zigzag). Stops on the first drop.
 *
 * @tparam Check The per-message check, invoked as `AdmissionDecision(VisitEvent const&)`.
 * @param body The packed field's bytes (the @c Bytes value of its event).
 * @param field The packed field's number, reported on every element.
 * @param check The per-message check to invoke.
 * @param options The cost of a malformed payload.
 * @return The check's drop, a drop for a truncated varint, or admit.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
visitPackedVarint(
    std::span<uint8_t const> body,
    uint64_t field,
    Check& check,
    ProtobufVisitorOptions const& options = {})
{
    auto pos = size_t{};
    while (pos < body.size())
    {
        auto varint = uint64_t{};
        if (not detail::readVarint(body, pos, varint))
        {
            return AdmissionDecision::drop(
                "Invalid protobuf payload", options.costForInvalidPayload);
        }
        if (auto const decision = check(
                VisitEvent{
                    .kind = EventKind::Int64,
                    .fieldNumber = field,
                    .value = static_cast<int64_t>(varint)});
            decision.dropped())
        {
            return decision;
        }
    }
    return AdmissionDecision::admit();
}

/**
 * @brief Packed visitor for 32-bit or 64-bit fixed elements (fixed32 / sfixed32 / float / fixed64 /
 * sfixed64 / double).
 *
 * Trailing bytes too short to hold a whole element are ignored.
 *
 * @tparam T The schema's element type, which fixes the element width and signedness.
 * @tparam Check The per-message check, invoked as `AdmissionDecision(VisitEvent const&)`.
 * @param body The packed field's bytes (the @c Bytes value of its event).
 * @param field The packed field's number, reported on every element.
 * @param check The per-message check to invoke.
 * @return The check's drop, or admit.
 * @see visitPackedVarint
 */
template <typename T, typename Check>
[[nodiscard]] AdmissionDecision
visitPackedFixed(std::span<uint8_t const> body, uint64_t field, Check& check)
{
    using Trait = detail::PackedTrait<T>;
    using ReadType = Trait::ReadType;
    using WriteType = Trait::WriteType;
    static constexpr auto kSize = sizeof(ReadType);

    for (auto pos = size_t{}; pos + kSize <= body.size(); pos += kSize)
    {
        auto element = ReadType{};
        std::memcpy(&element, &body[pos], kSize);
        // Reinterpret with the element's signedness (WriteType), then widen to the variant's
        // int64_t leaf so the check reads it the same way as any other scalar.
        if (auto const decision = check(
                VisitEvent{
                    .kind = EventKind::Int64,
                    .fieldNumber = field,
                    .value = static_cast<int64_t>(static_cast<WriteType>(element))});
            decision.dropped())
        {
            return decision;
        }
    }
    return AdmissionDecision::admit();
}

}  // namespace admission::spec
