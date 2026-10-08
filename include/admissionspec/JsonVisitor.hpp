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

struct DefaultJsonVisitorOptions
{
    static constexpr std::size_t maxArraySize = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t maxObjectSize = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t maxStringSize = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t maxKeySize = std::numeric_limits<std::size_t>::max();
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
 * Returning `false` from a callback stops the parse; the check's decision is left in @c decision.
 */
template <typename Check, typename VisitorOptions = DefaultJsonVisitorOptions>
struct JsonVisitor
{
    static constexpr std::size_t max_array_size = VisitorOptions::maxArraySize;
    static constexpr std::size_t max_object_size = VisitorOptions::maxObjectSize;
    static constexpr std::size_t max_string_size = VisitorOptions::maxStringSize;
    static constexpr std::size_t max_key_size = VisitorOptions::maxKeySize;

    JsonVisitor(Check& check, AdmissionDecision& decision) : check{check}, decision{decision}
    {
    }

    Check& check;
    AdmissionDecision& decision;
    /// Number of currently open containers.
    std::uint32_t depth{0};
    /// The key of each open container, innermost last.
    std::vector<std::string> containerKeys;
    /// The key naming the next value; empty inside arrays.
    std::string pendingKey;

    bool
    on_document_begin(boost::system::error_code& ec);

    bool
    on_document_end(boost::system::error_code& ec);

    bool
    on_array_begin(boost::system::error_code& ec);

    bool
    on_array_end(std::size_t n, boost::system::error_code& ec);

    bool
    on_object_begin(boost::system::error_code& ec);

    bool
    on_object_end(std::size_t n, boost::system::error_code& ec);

    bool
    on_string_part(std::string_view s, std::size_t n, boost::system::error_code& ec);

    bool
    on_string(std::string_view s, std::size_t n, boost::system::error_code& ec);

    bool
    on_key_part(std::string_view s, std::size_t n, boost::system::error_code& ec);

    bool
    on_key(std::string_view s, std::size_t n, boost::system::error_code& ec);

    bool
    on_number_part(std::string_view s, boost::system::error_code& ec);

    bool
    on_int64(int64_t i, std::string_view s, boost::system::error_code& ec);

    bool
    on_uint64(uint64_t u, std::string_view s, boost::system::error_code& ec);

    bool
    on_double(double d, std::string_view s, boost::system::error_code& ec);

    bool
    on_bool(bool b, boost::system::error_code& ec);

    bool
    on_null(boost::system::error_code& ec);

    bool
    on_comment_part(std::string_view s, boost::system::error_code& ec);

    bool
    on_comment(std::string_view s, boost::system::error_code& ec);

private:
    /// Hand @p event to the check, recording its decision. @return Whether to keep parsing.
    bool
    emit(VisitEvent const& event);

    /// Emit a leaf named by the pending key, which it consumes.
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
    decision = check(event);
    return decision.admitted();
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::emitLeaf(VisitEvent event)
{
    event.key = pendingKey;
    event.depth = depth;
    auto const keepGoing = emit(event);
    pendingKey.clear();
    return keepGoing;
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::beginContainer(EventKind kind)
{
    containerKeys.push_back(std::move(pendingKey));
    pendingKey.clear();
    return emit(VisitEvent{.kind = kind, .key = containerKeys.back(), .depth = depth++});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::endContainer(EventKind kind, std::size_t n)
{
    auto const keepGoing =
        emit(VisitEvent{.kind = kind, .key = containerKeys.back(), .size = n, .depth = --depth});
    containerKeys.pop_back();
    return keepGoing;
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_document_begin(boost::system::error_code&)
{
    return true;
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_document_end(boost::system::error_code&)
{
    return true;
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_array_begin(boost::system::error_code&)
{
    return beginContainer(EventKind::ArrayBegin);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_array_end(std::size_t n, boost::system::error_code&)
{
    return endContainer(EventKind::ArrayEnd, n);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_object_begin(boost::system::error_code&)
{
    return beginContainer(EventKind::ObjectBegin);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_object_end(std::size_t n, boost::system::error_code&)
{
    return endContainer(EventKind::ObjectEnd, n);
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_string_part(
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    // A partial piece does not consume the key: the final on_string still needs it.
    return emit(
        VisitEvent{
            .kind = EventKind::StringPart,
            .key = pendingKey,
            .size = n,
            .depth = depth,
            .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_string(
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::String, .size = n, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_key_part(
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    pendingKey += s;
    return emit(VisitEvent{.kind = EventKind::KeyPart, .size = n, .depth = depth, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_key(
    std::string_view s,
    std::size_t n,
    boost::system::error_code&)
{
    pendingKey += s;
    return emit(VisitEvent{.kind = EventKind::Key, .size = n, .depth = depth, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_number_part(std::string_view s, boost::system::error_code&)
{
    return emit(
        VisitEvent{.kind = EventKind::NumberPart, .key = pendingKey, .depth = depth, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_int64(
    int64_t i,
    std::string_view,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Int64, .value = i});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_uint64(
    uint64_t u,
    std::string_view,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Uint64, .value = u});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_double(
    double d,
    std::string_view,
    boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Double, .value = d});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_bool(bool b, boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Bool, .value = b});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_null(boost::system::error_code&)
{
    return emitLeaf(VisitEvent{.kind = EventKind::Null});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_comment_part(std::string_view s, boost::system::error_code&)
{
    return emit(VisitEvent{.kind = EventKind::CommentPart, .depth = depth, .value = s});
}

template <typename Check, typename VisitorOptions>
bool
JsonVisitor<Check, VisitorOptions>::on_comment(std::string_view s, boost::system::error_code&)
{
    return emit(VisitEvent{.kind = EventKind::Comment, .depth = depth, .value = s});
}

struct VisitJsonOptions
{
    double costForInvalidPayload{10};
    uint32_t maxDepth{std::numeric_limits<uint32_t>::max()};
};

/**
 * @brief Visit a serialized JSON message, emitting one @ref VisitEvent per SAX callback into
 *        @p check.
 *
 * Streams the payload through `boost::json::basic_parser` without building a DOM; see
 * @ref JsonVisitor for how keys, depth, and sizes are reported. Stops and returns on the first
 * drop.
 *
 * @param bytes The raw message payload.
 * @param check The per-message check to invoke.
 * @param options Parser limits and the cost of a malformed payload.
 * @return The check's drop, a drop for malformed JSON, or admit.
 */
template <typename Check, typename VisitorOptions = DefaultJsonVisitorOptions>
[[nodiscard]] AdmissionDecision
visitJson(std::span<uint8_t const> bytes, Check& check, VisitJsonOptions const& options = {})
{
    auto decision = AdmissionDecision::admit();
    auto parserOptions = boost::json::parse_options{.max_depth = options.maxDepth};
    auto parser = boost::json::basic_parser<JsonVisitor<Check, DefaultJsonVisitorOptions>>{
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
