/** @file */
#pragma once

#include <boost/json/basic_parser_impl.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <admissionspec/Types.hpp>

#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace admission::spec {

/**
 * @brief Compile-time size limits for a @ref JsonVisitor; the default imposes none.
 *
 * Supply a type with the same four members as the @c VisitorOptions argument of
 * @ref JsonVisitor / @ref visitJson to have the parser reject oversize containers, strings, or keys
 * as malformed JSON before any further event is emitted.
 */
struct DefaultJsonVisitorOptions
{
    /**
     * @brief Maximum number of elements in an array.
     */
    static constexpr std::size_t kMaxArraySize = std::numeric_limits<std::size_t>::max();

    /**
     * @brief Maximum number of members in an object.
     */
    static constexpr std::size_t kMaxObjectSize = std::numeric_limits<std::size_t>::max();

    /**
     * @brief Maximum length of a string value, in bytes.
     */
    static constexpr std::size_t kMaxStringSize = std::numeric_limits<std::size_t>::max();

    /**
     * @brief Maximum length of an object key, in bytes.
     */
    static constexpr std::size_t kMaxKeySize = std::numeric_limits<std::size_t>::max();
};

/**
 * @brief The `boost::json::basic_parser` handler behind @ref visitJson: translates each SAX
 *        callback into a @ref VisitEvent and hands it to the check.
 *
 * Keys are the only state carried between callbacks. A completed key names the next value; a
 * container keeps its key on a stack until it closes, so the matching @c ObjectEnd / @c ArrayEnd
 * reports it too. Array elements have no key. String, number, and comment text is never buffered:
 * each piece is reported as it arrives, with @c size carrying the running total.
 *
 * Every callback returns whether to keep parsing; `false` stops the parse, leaving the check's
 * decision in the @c AdmissionDecision passed at construction. The @c ec parameters are part of
 * the handler interface and are never set: a drop is reported through that decision instead.
 *
 * @tparam Check The per-message check, invoked as `AdmissionDecision(VisitEvent const&)`.
 * @tparam VisitorOptions The size limits; see @ref DefaultJsonVisitorOptions.
 */
template <typename Check, typename VisitorOptions = DefaultJsonVisitorOptions>
class JsonVisitor
{
    Check& check_;
    AdmissionDecision& decision_;
    std::uint32_t depth_{0};
    std::vector<std::string> containerKeys_;
    std::string pendingKey_;

public:
    JsonVisitor(JsonVisitor const&) = delete;
    JsonVisitor&
    operator=(JsonVisitor const&) = delete;
    JsonVisitor(JsonVisitor&&) = delete;
    JsonVisitor&
    operator=(JsonVisitor&&) = delete;

    // boost::json::basic_parser requires these exact names for its handler's limits and
    // callbacks.

    /**
     * @brief Maximum number of elements in an array; from @p VisitorOptions.
     */
    static constexpr std::size_t max_array_size =  // NOLINT(readability-identifier-naming)
        VisitorOptions::kMaxArraySize;

    /**
     * @brief Maximum number of members in an object; from @p VisitorOptions.
     */
    static constexpr std::size_t max_object_size =  // NOLINT(readability-identifier-naming)
        VisitorOptions::kMaxObjectSize;

    /**
     * @brief Maximum length of a string value, in bytes; from @p VisitorOptions.
     */
    static constexpr std::size_t max_string_size =  // NOLINT(readability-identifier-naming)
        VisitorOptions::kMaxStringSize;

    /**
     * @brief Maximum length of an object key, in bytes; from @p VisitorOptions.
     */
    static constexpr std::size_t max_key_size =  // NOLINT(readability-identifier-naming)
        VisitorOptions::kMaxKeySize;

    /**
     * @brief Construct a @ref JsonVisitor.
     *
     * @param check The check to hand each event to; must outlive the visitor.
     * @param decision Receives the check's most recent decision; must outlive the visitor.
     */
    JsonVisitor(Check& check, AdmissionDecision& decision) : check_{check}, decision_{decision}
    {
    }

    /**
     * @brief Start of the document; emits nothing.
     *
     * @param ec Unused.
     * @return Always true.
     */
    bool
    on_document_begin(boost::system::error_code& ec);  // NOLINT(readability-identifier-naming)

    /**
     * @brief End of the document; emits nothing.
     *
     * @param ec Unused.
     * @return Always true.
     */
    bool
    on_document_end(boost::system::error_code& ec);  // NOLINT(readability-identifier-naming)

    /**
     * @brief Emit @c ArrayBegin, keyed by the pending key, and open a container.
     *
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_array_begin(boost::system::error_code& ec);  // NOLINT(readability-identifier-naming)

    /**
     * @brief Close the innermost container and emit @c ArrayEnd with its key.
     *
     * @param n The number of elements in the array.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_array_end(  // NOLINT(readability-identifier-naming)
        std::size_t n,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c ObjectBegin, keyed by the pending key, and open a container.
     *
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_object_begin(boost::system::error_code& ec);  // NOLINT(readability-identifier-naming)

    /**
     * @brief Close the innermost container and emit @c ObjectEnd with its key.
     *
     * @param n The number of members in the object.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_object_end(  // NOLINT(readability-identifier-naming)
        std::size_t n,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c StringPart for a piece of a string value; the pending key is kept for the
     *        final piece.
     *
     * @param s This piece of the string.
     * @param n The length of the string so far, including @p s.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_string_part(  // NOLINT(readability-identifier-naming)
        std::string_view s,
        std::size_t n,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c String for the final piece of a string value, consuming the pending key.
     *
     * @param s The final piece of the string.
     * @param n The total length of the string.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_string(  // NOLINT(readability-identifier-naming)
        std::string_view s,
        std::size_t n,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c KeyPart for a piece of an object key and append it to the pending key.
     *
     * @param s This piece of the key.
     * @param n The length of the key so far, including @p s.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_key_part(  // NOLINT(readability-identifier-naming)
        std::string_view s,
        std::size_t n,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c Key for the final piece of an object key, completing the pending key that
     *        names the next value.
     *
     * @param s The final piece of the key.
     * @param n The total length of the key.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_key(  // NOLINT(readability-identifier-naming)
        std::string_view s,
        std::size_t n,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c NumberPart for a piece of a number's source text; the pending key is kept for
     *        the final value.
     *
     * @param s This piece of the number's text.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_number_part(  // NOLINT(readability-identifier-naming)
        std::string_view s,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c Int64, consuming the pending key.
     *
     * @param i The parsed value.
     * @param s The number's source text; unused.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_int64(  // NOLINT(readability-identifier-naming)
        int64_t i,
        std::string_view s,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c Uint64 (a value too large for @c int64_t), consuming the pending key.
     *
     * @param u The parsed value.
     * @param s The number's source text; unused.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_uint64(  // NOLINT(readability-identifier-naming)
        uint64_t u,
        std::string_view s,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c Double, consuming the pending key.
     *
     * @param d The parsed value.
     * @param s The number's source text; unused.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_double(  // NOLINT(readability-identifier-naming)
        double d,
        std::string_view s,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c Bool, consuming the pending key.
     *
     * @param b The parsed value.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_bool(bool b, boost::system::error_code& ec);  // NOLINT(readability-identifier-naming)

    /**
     * @brief Emit @c Null, consuming the pending key.
     *
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_null(boost::system::error_code& ec);  // NOLINT(readability-identifier-naming)

    /**
     * @brief Emit @c CommentPart for a piece of a comment (only when the parser allows comments).
     *
     * @param s This piece of the comment.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_comment_part(  // NOLINT(readability-identifier-naming)
        std::string_view s,
        boost::system::error_code& ec);

    /**
     * @brief Emit @c Comment for the final piece of a comment (only when the parser allows
     *        comments).
     *
     * @param s The final piece of the comment.
     * @param ec Unused.
     * @return Whether the check admitted the event.
     */
    bool
    on_comment(  // NOLINT(readability-identifier-naming)
        std::string_view s,
        boost::system::error_code& ec);

private:
    bool
    emit(VisitEvent const& event);

    bool
    emitLeaf(VisitEvent event);

    bool
    beginContainer(EventKind kind);

    bool
    endContainer(EventKind kind, std::size_t n);
};

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::emit(VisitEvent const& event)
{
    decision_ = check_(event);
    return decision_.admitted();
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::emitLeaf(VisitEvent event)
{
    event.key = pendingKey_;
    event.depth = depth_;
    auto const keepGoing = emit(event);
    pendingKey_.clear();
    return keepGoing;
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::beginContainer(EventKind kind)
{
    containerKeys_.push_back(std::move(pendingKey_));
    pendingKey_.clear();
    return emit(VisitEvent{.kind = kind, .key = containerKeys_.back(), .depth = depth_++});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::endContainer(EventKind kind, std::size_t n)
{
    auto const keepGoing =
        emit(VisitEvent{.kind = kind, .key = containerKeys_.back(), .size = n, .depth = --depth_});
    containerKeys_.pop_back();
    return keepGoing;
}

// boost::json::basic_parser calls its handler's members by these exact names.
template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_document_begin(  // NOLINT(readability-identifier-naming)
    boost::system::error_code&)
{
    return true;
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_document_end(  // NOLINT(readability-identifier-naming)
    boost::system::error_code&)
{
    return true;
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_array_begin(  // NOLINT(readability-identifier-naming)
    boost::system::error_code&)
{
    return beginContainer(EventKind::ArrayBegin);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_array_end(  // NOLINT(readability-identifier-naming)
    std::size_t n,
    boost::system::error_code&)
{
    return endContainer(EventKind::ArrayEnd, n);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_object_begin(  // NOLINT(readability-identifier-naming)
    boost::system::error_code&)
{
    return beginContainer(EventKind::ObjectBegin);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_object_end(  // NOLINT(readability-identifier-naming)
    std::size_t n,
    boost::system::error_code&)
{
    return endContainer(EventKind::ObjectEnd, n);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_string_part(  // NOLINT(readability-identifier-naming)
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    // A partial piece does not consume the key: the final on_string still needs it.
    return emit(
        VisitEvent{
            .kind = EventKind::StringPart,
            .key = pendingKey_,
            .size = n,
            .depth = depth_,
            .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_string(  // NOLINT(readability-identifier-naming)
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::String, .size = n, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_key_part(  // NOLINT(readability-identifier-naming)
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    pendingKey_ += s;
    return emit(VisitEvent{.kind = EventKind::KeyPart, .size = n, .depth = depth_, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_key(  // NOLINT(readability-identifier-naming)
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    pendingKey_ += s;
    return emit(VisitEvent{.kind = EventKind::Key, .size = n, .depth = depth_, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_number_part(  // NOLINT(readability-identifier-naming)
    std::string_view s,
    boost::system::error_code&)
{
    return emit(
        VisitEvent{.kind = EventKind::NumberPart, .key = pendingKey_, .depth = depth_, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_int64(  // NOLINT(readability-identifier-naming)
    int64_t i,
    std::string_view,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Int64, .value = i});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_uint64(  // NOLINT(readability-identifier-naming)
    uint64_t u,
    std::string_view,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Uint64, .value = u});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_double(  // NOLINT(readability-identifier-naming)
    double d,
    std::string_view,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Double, .value = d});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_bool(  // NOLINT(readability-identifier-naming)
    bool b,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Bool, .value = b});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_null(  // NOLINT(readability-identifier-naming)
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Null});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_comment_part(  // NOLINT(readability-identifier-naming)
    std::string_view s,
    boost::system::error_code&)
{
    return emit(VisitEvent{.kind = EventKind::CommentPart, .depth = depth_, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_comment(  // NOLINT(readability-identifier-naming)
    std::string_view s,
    boost::system::error_code&)
{
    return emit(VisitEvent{.kind = EventKind::Comment, .depth = depth_, .value = s});
}

/**
 * @brief Run-time options for @ref visitJson.
 */
struct VisitJsonOptions
{
    /**
     * @brief Token cost of the drop returned for a payload that is not valid JSON.
     */
    double costForInvalidPayload{10};

    /**
     * @brief Maximum nesting depth of arrays and objects; deeper input is treated as invalid JSON.
     */
    uint32_t maxDepth{std::numeric_limits<uint32_t>::max()};
};

/**
 * @brief Visit a serialized JSON message, emitting one @ref VisitEvent per SAX callback into
 *        @p check.
 *
 * Streams the payload through `boost::json::basic_parser` without building a DOM; see
 * @ref JsonVisitor for how keys, depth, and sizes are reported. Stops and returns on the first
 * drop. Malformed JSON, input exceeding a limit, and data after the end of the document are all
 * dropped as invalid.
 *
 * @tparam Check The per-message check, invoked as `AdmissionDecision(VisitEvent const&)`.
 * @tparam VisitorOptions The compile-time size limits; see @ref DefaultJsonVisitorOptions.
 * @param bytes The raw message payload.
 * @param check The per-message check to invoke.
 * @param options The nesting limit and the cost of a malformed payload.
 * @return The check's drop, a drop for invalid JSON, or admit.
 */
template <typename Check, typename VisitorOptions = DefaultJsonVisitorOptions>
[[nodiscard]] AdmissionDecision
visitJson(std::span<uint8_t const> bytes, Check& check, VisitJsonOptions const& options = {})
{
    auto decision = AdmissionDecision::admit();
    auto parserOptions = boost::json::parse_options{.max_depth = options.maxDepth};
    auto parser = boost::json::basic_parser<JsonVisitor<Check, VisitorOptions>>{
        parserOptions, check, decision};
    auto ec = boost::system::error_code{};
    try
    {
        auto const consumed =
            parser.write_some(false, reinterpret_cast<char const*>(bytes.data()), bytes.size(), ec);
        if (!ec && consumed != bytes.size())
        {
            ec = boost::json::error::extra_data;
        }
    }
    catch (boost::system::system_error const& e)
    {
        ec = e.code();
    }
    catch (...)
    {
        ec = boost::json::error::exception;
    }
    // A check that drops stops the parse, which then also reports an error — the drop wins.
    if (decision.dropped())
    {
        return decision;
    }
    if (ec)
    {
        return AdmissionDecision::drop("Invalid JSON payload", options.costForInvalidPayload);
    }
    return decision;
}

}  // namespace admission::spec
