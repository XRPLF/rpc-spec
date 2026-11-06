/** @file */
#pragma once

#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/RepeatedCapsCheck.hpp>
#include <admissionspec/detail/SpecLookup.hpp>

#include <type_traits>

namespace admission::spec {

/**
 * @brief Trait escape hatch for associating a spec with a foreign type @p T.
 *
 * For types you own, prefer an ADL-visible `consteval auto admissionSpec(std::type_identity<T>)`
 * overload in @p T's namespace. For foreign types, specialize this trait with a `static consteval
 * auto spec()`.
 */
template <typename T>
struct AdmissionTraits;  // primary template intentionally undefined

/**
 * @brief True when @p T has an associated admission spec (via ADL overload or trait
 * specialization).
 */
template <typename T>
concept HasAdmissionSpec = detail::HasAdmissionAdl<T> or detail::HasAdmissionTrait<T>;

/**
 * @brief Resolve the @ref AdmissionSpec associated with @p T.
 */
template <typename T>
[[nodiscard]] consteval auto
admissionSpecFor()
{
    if constexpr (detail::HasAdmissionAdl<T>)
    {
        return admissionSpec(std::type_identity<T>{});
    }
    else
    {
        static_assert(
            detail::HasAdmissionTrait<T>,
            "No AdmissionSpec for T: provide an ADL 'admissionSpec(std::type_identity<T>)' "
            "overload in "
            "T's namespace, or specialize util::admission::AdmissionTraits<T>.");
        return AdmissionTraits<T>::spec();
    }
}

/**
 * @brief The (otherwise unnameable) @ref AdmissionSpec type associated with @p T.
 */
template <typename T>
    requires HasAdmissionSpec<T>
using SpecOf = decltype(admissionSpecFor<T>());

/**
 * @brief The resolved-tunables type of @p T's spec (what a check's @c cfg argument is).
 */
template <typename T>
    requires HasAdmissionSpec<T>
using ResolvedOf = SpecOf<T>::Resolved;

/**
 * @brief The resolved tunables for type @p T.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] auto const&
resolvedFor()
{
    static auto const kResolved = admissionSpecFor<T>().resolveDefaults();
    return kResolved;
}

/**
 * @brief Make a fresh streaming checker for type @p T: its spec's check, bound to its resolved
 * tunables, behind its repeated-field caps (see @ref RepeatedCapsCheck).
 *
 * The binding point between a caller-provided payload visitor and the type's
 * @ref AdmissionSpec — call once per message to get an `AdmissionDecision(VisitEvent const&)`
 * callable, then feed it each event and stop on the first drop. See @ref admitBody.
 */
template <typename T>
    requires HasAdmissionSpec<T>
[[nodiscard]] auto
makeChecker()
{
    static constexpr auto kSpec = admissionSpecFor<T>();
    return RepeatedCapsCheck{kSpec.attributes().repeated, kSpec.makeChecker(resolvedFor<T>())};
}

}  // namespace admission::spec
