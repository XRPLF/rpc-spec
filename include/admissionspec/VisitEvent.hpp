/** @file */
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <variant>

namespace admission::spec {

/**
 * @brief The shape of one event in a streaming visit of a not-yet-hydrated payload.
 *
 * A visit is a flat stream of SAX-style events mirroring `boost::json::basic_parser`'s handler: a
 * typed leaf (@c String, @c Int64, @c Bool, ...), the @c Begin/@c End of a container, or a partial
 * piece of a string, key, number, or comment. Protobuf adds @c Bytes for a length-delimited field.
 * The framework reports no path or index; a check that needs more structural context ("the value
 * under key X inside object Y") keeps its own state across the events of a single message; see
 * @ref AdmissionSpec::withCheck.
 */
enum class EventKind : uint8_t {
    ObjectBegin,  ///< on_object_begin
    ObjectEnd,    ///< on_object_end      (size = member count)
    ArrayBegin,   ///< on_array_begin
    ArrayEnd,     ///< on_array_end       (size = element count)
    KeyPart,      ///< on_key_part        (value = piece, size = running total)
    Key,          ///< on_key             (value = final piece, size = total)
    StringPart,   ///< on_string_part     (value = piece, size = running total)
    String,       ///< on_string          (value = final piece, size = total)
    NumberPart,   ///< on_number_part     (value = piece)
    Int64,        ///< on_int64           (value = int64_t)
    Uint64,       ///< on_uint64          (value = uint64_t)
    Double,       ///< on_double          (value = double)
    Bool,         ///< on_bool            (value = bool)
    Null,         ///< on_null
    CommentPart,  ///< on_comment_part    (value = piece)
    Comment,      ///< on_comment         (value = final piece)
    Bytes,        ///< protobuf length-delimited field (value = span); no JSON equivalent
};

/**
 * @brief An event handed to a streaming admission check as a visitor decodes a payload.
 *
 * The @ref ConnectionLimiter hands a caller-provided visitor a per-message check bound to the
 * type's
 * @ref AdmissionSpec; the visitor emits one @c VisitEvent per node it decodes and the check decides
 * admit/drop — so a message can be rejected before it is ever fully hydrated.
 */
struct VisitEvent
{
    /**
     * @brief The kind of node: a typed leaf, a partial piece, or the begin/end of a container.
     */
    EventKind kind{EventKind::Null};

    /**
     * @brief JSON key of this node; empty for protobuf and for array elements.
     */
    std::string_view key;

    /**
     * @brief The size of this node.
     *
     * The member or element count on @c ObjectEnd / @c ArrayEnd; the length in bytes, so far or in
     * total, of a string or key.
     */
    std::size_t size{};

    /**
     * @brief Nesting depth of this node in the message; 0 for the top-level message.
     */
    std::uint32_t depth{};

    /**
     * @brief Protobuf field number of this node; max uint64_t for JSON.
     */
    uint64_t fieldNumber{std::numeric_limits<uint64_t>::max()};

    /**
     * @brief The leaf payload.
     *
     * @c monostate for containers and @c Null; a @c string_view for the (partial) text of a
     * string, key, number, or comment; a span over its bytes for a @c Bytes event (a protobuf
     * bytes field, or a passthrough payload).
     */
    std::variant<
        std::monostate,
        bool,
        int64_t,
        uint64_t,
        double,
        std::string_view,
        std::span<uint8_t const>>
        value;

    /**
     * @brief Access the leaf payload as a @p U.
     *
     * @tparam U The alternative of @c value to read.
     * @return Pointer to the value if it holds a @p U, else nullptr.
     */
    template <typename U>
    [[nodiscard]] constexpr U const*
    as() const noexcept
    {
        return std::get_if<U>(&value);
    }
};

}  // namespace admission::spec
