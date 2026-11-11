/** @file */
#pragma once

#include <type_traits>

namespace admission::spec {

template <typename T>
struct AdmissionTraits;

namespace detail {

/**
 * @brief Whether @p T has an ADL-visible `admissionSpec(std::type_identity<T>)` overload.
 */
template <typename T>
concept HasAdmissionAdl = requires { admissionSpec(std::type_identity<T>{}); };

/**
 * @brief Whether @ref AdmissionTraits is specialized for @p T.
 */
template <typename T>
concept HasAdmissionTrait = requires { AdmissionTraits<T>::spec(); };

}  // namespace detail
}  // namespace admission::spec
