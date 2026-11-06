/** @file */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace admission::spec {

/**
 * @brief A cap on a repeated field, enforced while the payload is walked, before it is parsed.
 *
 * It names the field by its protobuf number or by its JSON key; set exactly one. A protobuf cap
 * counts the top-level message's entries of that field, which need not be contiguous. A JSON cap
 * counts the elements directly inside an array with that key, wherever it appears, per array.
 */
struct RepeatedCap
{
    /**
     * @brief The field's number in a protobuf schema, or 0 for a JSON cap.
     */
    uint32_t fieldNumber{};

    /**
     * @brief The array's key in a JSON message, or empty for a protobuf cap.
     */
    std::string_view key;

    /**
     * @brief The most entries the field may hold.
     */
    uint64_t maxEntries{};

    /**
     * @brief The fewest bytes one entry may take, so a count under the cap cannot be padded out
     * with empty entries. It applies to string and bytes entries, and to protobuf sub-messages.
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
     * @return The cap on protobuf field @p fieldNumber, or nothing if the field is not capped.
     */
    [[nodiscard]] constexpr std::optional<RepeatedCap>
    find(uint32_t fieldNumber) const noexcept
    {
        for (size_t i{}; i < count; ++i)
        {
            if (caps[i].fieldNumber != 0 and caps[i].fieldNumber == fieldNumber)
            {
                return caps[i];
            }
        }
        return std::nullopt;
    }

    /**
     * @return The cap on the JSON array @p key, or nothing if it is not capped.
     */
    [[nodiscard]] constexpr std::optional<RepeatedCap>
    find(std::string_view key) const noexcept
    {
        for (size_t i{}; i < count; ++i)
        {
            if (not caps[i].key.empty() and caps[i].key == key)
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
 * `repeatedCaps({{.fieldNumber = 3, .maxEntries = 100, .minEntryBytes = 1}})` for protobuf, or
 * `repeatedCaps({{.key = "items", .maxEntries = 100}})` for JSON.
 */
template <size_t N>
    requires(N <= kMaxRepeatedCaps)
[[nodiscard]] consteval RepeatedCaps
repeatedCaps(RepeatedCap const (&caps)[N])
{
    auto result = RepeatedCaps{};
    for (size_t i{}; i < N; ++i)
    {
        if ((caps[i].fieldNumber != 0) == (not caps[i].key.empty()))
        {
            throw "repeatedCaps: a cap names exactly one of a field number or a key";
        }
        for (size_t j{}; j < i; ++j)
        {
            if ((caps[i].fieldNumber != 0 and caps[j].fieldNumber == caps[i].fieldNumber) or
                (not caps[i].key.empty() and caps[j].key == caps[i].key))
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
