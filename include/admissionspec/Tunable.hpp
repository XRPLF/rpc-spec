/** @file */
#pragma once

#include <admissionspec/FixedString.hpp>
#include <admissionspec/SizeCostRamp.hpp>

#include <string_view>
#include <vector>

namespace admission::spec {

/**
 * @brief A named, config-overridable configuration value — the single currency of an @ref
 * AdmissionSpec.
 *
 * Every configurable value (the hard drop cap, the size→cost ramp, and any hook thresholds) is a
 * @c Tunable: a compile-time default plus the config-file key that overrides it, tagged with a
 * compile-time @p Name so it can be looked up type-safely after resolution.
 *
 * @tparam Name Compile-time identifier (e.g. "max_payload_bytes", "size_ramp", "max_entries").
 * @tparam T Default value type.
 */
template <FixedString Name, typename T>
struct Tunable
{
    /**
     * @brief The value this converter produces (`T`).
     */
    using ValueType = T;

    /**
     * @brief Identifier for this item in the schema dump ("kName").
     */
    static constexpr std::string_view kName = Name.view();

    /**
     * @brief Value used when the config supplies no override.
     */
    T defaultValue{};

    /**
     * @brief Config key this tunable reads its override from.
     */
    std::string_view configKey;

    /**
     * @brief Compare two values of this type.
     *
     * @return The comparison result.
     */
    bool
    operator==(Tunable const&) const = default;
};

/**
 * @brief Build a named @ref Tunable, e.g.
 *        @code tunable<"max_entries">(size_t{100}, "admission.x.max_entries") @endcode
 */
template <FixedString Name, typename T>
[[nodiscard]] consteval Tunable<Name, T>
tunable(T defaultValue, std::string_view configKey) noexcept
{
    return Tunable<Name, T>{defaultValue, configKey};
}

}  // namespace admission::spec
