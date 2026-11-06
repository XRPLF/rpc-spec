/** @file */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace admission::spec {

/**
 * @brief A cap on a repeated field, checked while the payload is walked, before it is parsed.
 */
struct RepeatedCap
{
    /**
     * @brief The field's number in the message's schema.
     */
    uint32_t fieldNumber{};

    /**
     * @brief The most entries the field may hold.
     */
    uint64_t maxEntries{};

    /**
     * @brief The fewest bytes one entry may take; smaller entries are refused, so a count under
     * the cap cannot be padded out with empty entries.
     */
    uint64_t minEntryBytes{};

    constexpr bool
    operator==(RepeatedCap const&) const = default;
};

/**
 * @brief The most repeated-field caps one spec may declare.
 */
inline constexpr size_t kMaxRepeatedCaps = 8;

/**
 * @brief A spec's repeated-field caps.
 */
struct RepeatedCaps
{
    std::array<RepeatedCap, kMaxRepeatedCaps> caps{};
    size_t count{};

    /**
     * @return The cap on @p fieldNumber, or nothing if the field is not capped.
     */
    [[nodiscard]] constexpr std::optional<RepeatedCap>
    find(uint32_t fieldNumber) const noexcept
    {
        for (size_t i{}; i < count; ++i)
        {
            if (caps[i].fieldNumber == fieldNumber)
            {
                return caps[i];
            }
        }
        return std::nullopt;
    }

    constexpr bool
    operator==(RepeatedCaps const&) const = default;
};

/**
 * @brief Build @ref RepeatedCaps from a braced list, e.g.
 * `repeatedCaps({{.fieldNumber = 3, .maxEntries = 12288, .minEntryBytes = 1}})`.
 */
template <size_t N>
    requires(N <= kMaxRepeatedCaps)
[[nodiscard]] consteval RepeatedCaps
repeatedCaps(RepeatedCap const (&caps)[N])
{
    auto result = RepeatedCaps{};
    for (size_t i{}; i < N; ++i)
    {
        for (size_t j{}; j < i; ++j)
        {
            if (caps[j].fieldNumber == caps[i].fieldNumber)
            {
                throw "repeatedCaps: a field is capped twice";  // a compile error in consteval
            }
        }
        result.caps[i] = caps[i];
    }
    result.count = N;
    return result;
}

}  // namespace admission::spec
