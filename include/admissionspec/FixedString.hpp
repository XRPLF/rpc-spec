/** @file */
#pragma once

#include <cstddef>
#include <string_view>

namespace admission::spec {

/**
 * @brief A compile-time string usable as a non-type template parameter (for naming tunables).
 */
template <size_t N>
struct FixedString
{
    /**
     * @brief The stored characters, including the trailing NUL.
     */
    char value[N]{};

    /**
     * @brief Construct a @ref FixedString.
     *
     * @param str The string literal to store.
     */
    consteval FixedString(char const (&str)[N]) noexcept
    {
        for (size_t i{}; i < N; ++i)
        {
            value[i] = str[i];
        }
    }

    /**
     * @brief The string this fixed-string holds.
     *
     * @return A view over the stored characters.
     */
    [[nodiscard]] constexpr std::string_view
    view() const noexcept
    {
        return std::string_view{value, N - 1};
    }

    /**
     * @brief Compare two values of this type.
     *
     * @return The comparison result.
     */
    bool
    operator==(FixedString const&) const = default;
};

template <size_t N>
FixedString(char const (&)[N]) -> FixedString<N>;

}  // namespace admission::spec
