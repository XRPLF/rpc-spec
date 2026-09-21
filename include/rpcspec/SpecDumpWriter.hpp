/** @file */
#pragma once

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace rpc::spec {

/**
 * @brief Indented YAML-ish writer used by the spec dumper.
 *
 * Output shape example:
 *
 *   - account:
 *       - account
 *   - signer_lists:
 *       - type
 *           of: bool
 */
class SpecDumpWriter
{
    std::ostream* os_;
    int indent_ = 0;

public:
    /**
     * @brief Construct a @ref SpecDumpWriter.
     *
     * @param os The stream to write to; must outlive this writer.
     */
    explicit SpecDumpWriter(std::ostream& os) noexcept : os_{&os}
    {
    }

    /**
     * @brief Increase the current indentation level by one step.
     */
    void
    push() noexcept
    {
        ++indent_;
    }

    /**
     * @brief Decrease the current indentation level by one step.
     */
    void
    pop() noexcept
    {
        --indent_;
    }

    /**
     * @brief Return the underlying output stream.
     * @return A reference to the stream passed at construction.
     */
    [[nodiscard]] std::ostream&
    stream() const noexcept
    {
        return *os_;
    }

    /**
     * @brief Emit a top-of-line "key:" header followed by an indented block built by @p body.
     *
     * @tparam Fn The callable rendering the block body.
     * @param key The header name, emitted as "key:".
     * @param body Invoked once at the increased indent level.
     */
    template <typename Fn>
        requires std::invocable<Fn&>
    void
    header(std::string_view key, Fn&& body)
    {
        writeIndent();
        *os_ << key << ":\n";
        push();
        body();
        pop();
    }

    /**
     * @brief Emit a list bullet "- name" line, then run @p body indented under it.
     *
     * @tparam Fn The callable rendering the nested block.
     * @param name The bullet text.
     * @param body Invoked once at the increased indent level.
     */
    template <typename Fn>
        requires std::invocable<Fn&>
    void
    bullet(std::string_view name, Fn&& body)
    {
        writeIndent();
        *os_ << "- " << name << '\n';
        push();
        body();
        pop();
    }

    /**
     * @brief Emit "- name:" for a field-like bullet and run @p body indented under it.
     *
     * @tparam Fn The callable rendering the nested block.
     * @param name The bullet name, emitted as "- name:".
     * @param body Invoked once at the increased indent level.
     */
    template <typename Fn>
        requires std::invocable<Fn&>
    void
    bulletGroup(std::string_view name, Fn&& body)
    {
        writeIndent();
        *os_ << "- " << name << ":\n";
        push();
        body();
        pop();
    }

    /**
     * @brief Emit a "key: value" parameter line at the current indent.
     *
     * @tparam T The value type; formatted by `writeScalar`.
     * @param key The parameter name.
     * @param value The value to format.
     */
    template <typename T>
    void
    param(std::string_view key, T const& value)
    {
        writeIndent();
        *os_ << key << ": ";
        writeScalar(value);
        *os_ << '\n';
    }

    /**
     * @brief Emit a "key: [a, b, c]" parameter line at the current indent.
     *
     * @tparam Range A range of values, each formatted by `writeScalar`.
     * @param key The parameter name.
     * @param values The values to format as an inline list.
     */
    template <typename Range>
    void
    paramList(std::string_view key, Range const& values)
    {
        writeIndent();
        *os_ << key << ": [";
        bool first = true;
        for (auto const& value : values)
        {
            if (not first)
                *os_ << ", ";
            writeScalar(value);
            first = false;
        }
        *os_ << "]\n";
    }

    /**
     * @brief Emit a "key: [a, b, c]" parameter line from a brace-enclosed initializer list.
     *
     * @tparam T The element type of the initializer list.
     * @param key The parameter name to emit.
     * @param values The values to format as an inline list.
     */
    template <typename T>
    void
    paramList(std::string_view key, std::initializer_list<T> values)
    {
        paramList<std::initializer_list<T>>(key, values);
    }

    /**
     * @brief Emit a single-line plain line at the current indent.
     *
     * @param text The text to emit verbatim.
     */
    void
    line(std::string_view text)
    {
        writeIndent();
        *os_ << text << '\n';
    }

private:
    void
    writeIndent() const
    {
        for (auto i = 0; i < indent_; ++i)
            *os_ << "  ";
    }

    template <typename T>
    void
    writeScalar(T const& value) const
    {
        using D = std::decay_t<T>;
        if constexpr (std::is_same_v<D, bool>)
        {
            *os_ << (value ? "true" : "false");
        }
        else if constexpr (
            std::is_same_v<D, std::string_view> or std::is_same_v<D, std::string> or
            std::is_same_v<D, char const*>)
        {
            *os_ << '"' << value << '"';
        }
        else
        {
            *os_ << value;
        }
    }
};

}  // namespace rpc::spec
