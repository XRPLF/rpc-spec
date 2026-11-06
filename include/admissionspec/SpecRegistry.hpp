/** @file */
#pragma once

#include <admissionspec/SpecLookup.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace admission::spec {

/**
 * @brief The type number @p T's spec declares.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] consteval uint32_t
typeIdOf()
{
    constexpr auto kTypeId = admissionSpecFor<T>().attributes().typeId;
    static_assert(kTypeId.has_value(), "SpecRegistry: a spec in a registry must declare a typeId");
    return *kTypeId;
}

/**
 * @brief Maps a frame's type number to the type whose spec governs it.
 *
 * @code
 * using PeerSpecs = SpecRegistry<TMPing, TMManifests, TMGetLedger>;
 * auto const result = PeerSpecs::dispatch(
 *     frame.type,
 *     [&]<class T> { return admitHeader<T>(frame, connection, sink); },
 *     [&] { return StageResult{AdmissionDecision::drop("unknown type", PenaltyClass::Useless)}; });
 * @endcode
 */
template <typename... Ts>
    requires(HasAdmissionSpec<Ts> and ...)
struct SpecRegistry
{
    /**
     * @brief Whether every type number in the registry is distinct.
     */
    [[nodiscard]] static consteval bool
    distinctIds()
    {
        constexpr auto kIds = std::array<uint32_t, sizeof...(Ts)>{typeIdOf<Ts>()...};
        for (size_t i = 0; i < kIds.size(); ++i)
        {
            for (size_t j = i + 1; j < kIds.size(); ++j)
            {
                if (kIds[i] == kIds[j])
                {
                    return false;
                }
            }
        }
        return true;
    }

    static_assert(distinctIds(), "SpecRegistry: two specs declare the same typeId");

    /**
     * @brief Call @p known with the type @p type names, or @p unknown if it names none.
     *
     * @param type The type number from the frame header.
     * @param known Called as `known.template operator()<T>()`.
     * @param unknown Called with no arguments. Its result type is the result type of every call.
     * @return What the call returns.
     */
    template <typename Known, typename Unknown>
    static auto
    dispatch(uint32_t type, Known&& known, Unknown&& unknown) -> std::invoke_result_t<Unknown&>
    {
        using Result = std::invoke_result_t<Unknown&>;
        if constexpr (std::is_void_v<Result>)
        {
            bool const found =
                ((type == typeIdOf<Ts>() ? (known.template operator()<Ts>(), true) : false) or ...);
            if (not found)
            {
                unknown();
            }
        }
        else
        {
            Result result{};
            bool const found =
                ((type == typeIdOf<Ts>() ? (result = known.template operator()<Ts>(), true)
                                         : false) or
                 ...);
            if (not found)
            {
                return unknown();
            }
            return result;
        }
    }
};

}  // namespace admission::spec
