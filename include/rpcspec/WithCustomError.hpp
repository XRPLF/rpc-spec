/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Types.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace rpc::spec {

/**
 * @brief A meta-processor that wraps a requirement or modifier and substitutes a custom
 * @ref rpc::Status when the wrapped processor fails.
 *
 * The custom error is expressed as a @ref rpc::CombinedError code plus an optional message
 * string. The @ref rpc::Status is constructed lazily on the error path only.
 *
 * @tparam Wrapped A type satisfying @ref SomeRequirement or @ref SomeModifier.
 */
template <typename Wrapped>
    requires SomeRequirement<Wrapped> or SomeModifier<Wrapped>
class WithCustomError
{
    Wrapped wrapped_;
    rpc::CombinedError code_;
    std::string_view message_;  // empty -> use Status{code} only

public:
    /**
     * @brief Identifier for this item in the schema dump ("withCustomError").
     */
    static constexpr std::string_view kName = "withCustomError";

    /**
     * @brief Constructs a WithCustomError wrapper.
     *
     * @param wrapped The requirement or modifier to run.
     * @param code The error code to report when @p w fails.
     * @param message An optional message appended to the status (defaults to empty).
     *                Should point at static storage in normal usage.
     */
    consteval WithCustomError(
        Wrapped wrapped,
        rpc::CombinedError code,
        std::string_view message = {})
        : wrapped_{std::move(wrapped)}, code_{code}, message_{message}
    {
    }

    /**
     * @brief The processor whose error is being replaced.
     *
     * @return A reference to the wrapped processor.
     */
    [[nodiscard]] Wrapped const&
    wrapped() const noexcept
    {
        return wrapped_;
    }

    /**
     * @brief The replacement message, if any.
     *
     * @return The message; empty when only the code is replaced.
     */
    [[nodiscard]] std::string_view
    message() const noexcept
    {
        return message_;
    }

    /**
     * @brief Runs the wrapped requirement and returns the custom error if it fails.
     *
     * @param fieldView Field view for the field under validation.
     * @return Empty on success; the custom @ref rpc::Status on failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
        requires SomeRequirement<Wrapped>
    {
        if (auto const result = wrapped_.verify(fieldView); not result)
            return std::unexpected{makeStatus()};
        return {};
    }

    /**
     * @brief Runs the wrapped modifier and returns the custom error if it fails.
     *
     * @param fieldView Mutable field view for the field under modification.
     * @return Empty on success; the custom @ref rpc::Status on failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    modify(View& fieldView) const
        requires SomeModifier<Wrapped>
    {
        if (auto const result = wrapped_.modify(fieldView); not result)
            return std::unexpected{makeStatus()};
        return {};
    }

private:
    [[nodiscard]] rpc::Status
    makeStatus() const
    {
        return message_.empty() ? rpc::Status{code_} : rpc::Status{code_, std::string{message_}};
    }
};

}  // namespace rpc::spec
