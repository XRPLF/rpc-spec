/** @file */
#pragma once

#include <admissionspec/FixedString.hpp>
#include <admissionspec/Tunable.hpp>
#include <admissionspec/detail/ResolvedType.hpp>
#include <admissionspec/detail/TunableIndex.hpp>

#include <cstddef>
#include <tuple>
#include <utility>

namespace admission::spec {

/**
 * @brief The resolved values of a spec's tunables, looked up by compile-time name.
 *
 * Produced by resolving each @ref Tunable against the config (or its default). Both the eval stages
 * and the author hooks query it the same way: @code resolved.get<"size_ramp">() @endcode.
 *
 * @tparam Tunables The @ref Tunable types the owning spec declared.
 */
template <typename... Tunables>
class ResolvedTunables
{
    std::tuple<detail::ResolvedTypeOfT<typename Tunables::ValueType>...> values_;

public:
    /**
     * @brief Construct a @ref ResolvedTunables.
     *
     * @param vals One resolved value per declared tunable, in declaration order.
     */
    constexpr explicit ResolvedTunables(
        detail::ResolvedTypeOfT<typename Tunables::ValueType>... vals)
        : values_{std::move(vals)...}
    {
    }

    /**
     * @brief Resolved value of the tunable named @p Name.
     *
     * @tparam Name The tunable's declared name; a typo is a compile error.
     * @return A reference to the resolved value.
     */
    template <FixedString Name>
    [[nodiscard]] constexpr auto const&
    get() const noexcept
    {
        constexpr size_t kIdx = detail::tunableIndex<Name, Tunables...>();
        static_assert(kIdx < sizeof...(Tunables), "ResolvedTunables::get: unknown tunable name");
        return std::get<kIdx>(values_);
    }

    /**
     * @brief Whether a tunable named @p Name was declared.
     *
     * @tparam Name The tunable name to look for.
     * @return true when the owning spec declared it; false otherwise.
     */
    template <FixedString Name>
    [[nodiscard]] static constexpr bool
    has() noexcept
    {
        return detail::tunableIndex<Name, Tunables...>() < sizeof...(Tunables);
    }
};

/**
 * @brief Maps a `std::tuple<Tunable...>` to the corresponding @ref ResolvedTunables type.
 */
template <typename TunablesTuple>
struct ResolvedTunablesOf;

/**
 * @brief Maps a tuple of tunables to the matching @ref ResolvedTunables type.
 */
template <typename... Tunables>
struct ResolvedTunablesOf<std::tuple<Tunables...>>
{
    /**
     * @brief The resolved type.
     */
    using type = ResolvedTunables<Tunables...>;
};

template <typename TunablesTuple>
using ResolvedTunablesOfT = ResolvedTunablesOf<TunablesTuple>::type;

/**
 * @brief Build a @ref ResolvedTunables from a tunable tuple using each tunable's default value.
 */
template <typename... Tunables>
[[nodiscard]] inline ResolvedTunables<Tunables...>
resolveTunableDefaults(std::tuple<Tunables...> const& tunables)
{
    return std::apply(
        [](Tunables const&... tunable) {
            return ResolvedTunables<Tunables...>{detail::toResolved(tunable.defaultValue)...};
        },
        tunables);
}

}  // namespace admission::spec
