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

template <>
struct PackedTrait<uint32_t>
{
    using ReadType = uint32_t;
    using WriteType = int32_t;
};

template <>
struct PackedTrait<int32_t>
{
    using ReadType = uint32_t;
    using WriteType = int32_t;
};

template <>
struct PackedTrait<uint64_t>
{
    using ReadType = uint64_t;
    using WriteType = int64_t;
};

template <>
struct PackedTrait<int64_t>
{
    using ReadType = uint64_t;
    using WriteType = int64_t;
};

enum class WireType {
    Varint = 0,  ///< Variable int32_t, int64_t, uint32_t, uint64_t, bool, enum
    I64 = 1,     ///< int64_t, uint64_t, double
    Len = 2,     ///< string, bytes, sub messages, repeated fields
    I32 = 5,     ///< int32_t, uint32_t, float
};

/** Read a base-128 varint from @p bytes at @p pos, advancing it. Returns false if truncated. */
[[nodiscard]] inline bool
readVarint(std::span<uint8_t const> bytes, size_t& pos, uint64_t& out)
{
    auto result = uint64_t{};
    auto shift = uint64_t{};
    while (pos < bytes.size())
    {
        auto const b = bytes[pos++];
        result |= static_cast<uint64_t>(b & 0x7F) << shift;
        if ((b & 0x80) == 0)
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
 * @brief Visit a serialized protobuf message, emitting one @ref VisitEvent per field into @p check.
 *
 * Scalars (varint / 32- / 64-bit) are reported with their value. A length-delimited field is
 * ambiguous on the wire — string, packed list, or sub-message — so the visitor does not guess: it
 * reports the field's @c value as a span over exactly that field's bytes. The spec author, who has
 * the schema, decides what to do with it: read it as a scalar, or re-enter over the span with @ref
 * visitProtobuf (sub-message), @ref visitPackedVarint, or @ref visitPackedFixed (packed list).
 * Stops and returns on the first drop.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
visitProtobuf(std::span<uint8_t const> bytes, Check& check, double costForInvalidPayload = 10)
{
    auto pos = size_t{};
    while (pos < bytes.size())
    {
        auto tag = uint64_t{};
        if (!detail::readVarint(bytes, pos, tag))
        {
            return AdmissionDecision::drop("Invalid protobuf payload", costForInvalidPayload);
        }
        auto const field = static_cast<uint64_t>(tag >> 3);
        auto const wireType = static_cast<uint64_t>(tag & 0x07);

        auto scalar = [&](int64_t v) {
            return check(VisitEvent{.kind = EventKind::Scalar, .fieldNumber = field, .value = v});
        };

        auto handleInt = [&](auto size) {
            if (pos + size > bytes.size())
            {
                return AdmissionDecision::drop("Invalid protobuf payload", costForInvalidPayload);
            }
            auto v = uint64_t{};
            std::memcpy(&v, &bytes[pos], size);
            pos += size;
            return scalar(static_cast<int64_t>(v));
        };

        switch (static_cast<detail::WireType>(wireType))
        {
            using enum detail::WireType;
            case Varint: {
                auto v = uint64_t{};
                if (!detail::readVarint(bytes, pos, v))
                {
                    return AdmissionDecision::drop(
                        "Invalid protobuf payload", costForInvalidPayload);
                }
                if (auto const d = scalar(static_cast<int64_t>(v)); d.dropped())
                {
                    return d;
                }
            }
            break;
            case I64: {
                if (auto const d = handleInt(8); d.dropped())
                {
                    return d;
                }
            }
            break;
            case I32: {
                if (auto const d = handleInt(4); d.dropped())
                {
                    return d;
                }
            }
            break;
            case Len: {
                auto len = uint64_t{};
                if (!detail::readVarint(bytes, pos, len) || pos + len > bytes.size())
                {
                    return AdmissionDecision::drop(
                        "Invalid protobuf payload", costForInvalidPayload);
                }
                auto const body = bytes.subspan(pos, static_cast<size_t>(len));
                pos += static_cast<size_t>(len);

                // Report the raw span. Its meaning (string / packed list / sub-message) is schema,
                // so the author decides: read it as a scalar, or re-enter with visitProtobuf /
                // visitPackedVarint / visitPackedFixed over these bytes.
                if (auto const d = check(
                        VisitEvent{.kind = EventKind::Scalar, .fieldNumber = field, .value = body});
                    d.dropped())
                {
                    return d;
                }
            }
            break;
        }
    }
    return AdmissionDecision::admit();
}

/**
 * @brief Re-enter over a packed @c repeated payload, emitting each element as a @c Scalar of @p
 * field.
 *
 * A packed field's bytes are the element values concatenated with no tags, so the message visitor
 * cannot parse them — but the element wire type is known from the schema, so the spec author picks
 * the matching visitor. This is a flat scan of one level (not a recursive descent): each element is
 * a sibling, so to the check the stream is identical to an *unpacked* repeated field. Use for
 * packed varint types (int32/64, uint32/64, bool, enum, sint via zigzag). Stops on the first drop.
 */
template <typename Check>
[[nodiscard]] AdmissionDecision
visitPackedVarint(
    std::span<uint8_t const> body,
    uint64_t field,
    Check& check,
    double costForInvalidPayload = 10)
{
    auto pos = size_t{};
    while (pos < body.size())
    {
        auto v = uint64_t{};
        if (!detail::readVarint(body, pos, v))
        {
            return AdmissionDecision::drop("Invalid protobuf payload", costForInvalidPayload);
        }
        if (auto const d = check(
                VisitEvent{
                    .kind = EventKind::Scalar,
                    .fieldNumber = field,
                    .value = static_cast<int64_t>(v)});
            d.dropped())
        {
            return d;
        }
    }
    return AdmissionDecision::admit();
}

/**
 * @brief Packed visitor for 32-bit or 64-bit fixed elements (fixed32 / sfixed32 / float / fixed64 /
 * sfixed64 / double).
 * @see visitPackedVarint
 */
template <typename T, typename Check>
[[nodiscard]] AdmissionDecision
visitPackedFixed(std::span<uint8_t const> body, uint64_t field, Check& check)
{
    using Trait = detail::PackedTrait<T>;
    using ReadType = typename Trait::ReadType;
    using WriteType = typename Trait::WriteType;
    static constexpr auto Size = sizeof(ReadType);

    for (auto pos = size_t{}; pos + Size <= body.size(); pos += Size)
    {
        auto v = ReadType{};
        std::memcpy(&v, &body[pos], Size);
        // Reinterpret with the element's signedness (WriteType), then widen to the variant's
        // int64_t leaf so the check reads it the same way as any other scalar.
        if (auto const d = check(
                VisitEvent{
                    .kind = EventKind::Scalar,
                    .fieldNumber = field,
                    .value = static_cast<int64_t>(static_cast<WriteType>(v))});
            d.dropped())
        {
            return d;
        }
    }
    return AdmissionDecision::admit();
}

}  // namespace admission::spec
