/** @file */
#pragma once
// Compile-time server-conditional validator wrappers for the rpcspec DSL.
//
// Exactly one of RPCSPEC_IS_CLIO or RPCSPEC_IS_RIPPLED must be defined by the
// consuming project's build system (set automatically via each project's
// conanfile).
//
// ifServerClio(v...)    — applies validators only in Clio builds
// ifServerRippled(v...) — applies validators only in rippled builds
//
// Each wrapper satisfies the union of the inner validators' concepts:
//   SomeRequirement<IfServerClioValidator<Vs...>>  iff (SomeRequirement<Vs> ||
//   ...) SomeCheck<IfServerClioValidator<Vs...>>        iff (SomeCheck<Vs> ||
//   ...) SomeModifier<IfServerClioValidator<Vs...>>     iff (SomeModifier<Vs>
//   || ...)
//
// Multiple validators may be passed; they are applied in order:
//   field("full", ifServerClio(notSupportedIf(true), deprecated))
//   field("diff", ifServerClio(type<bool>))

#if !defined(RPCSPEC_IS_CLIO) && !defined(RPCSPEC_IS_RIPPLED)
#error                                                                         \
    "rpcspec: must define RPCSPEC_IS_CLIO=1 or RPCSPEC_IS_RIPPLED=1 (set in your project's conanfile)"
#endif
#if defined(RPCSPEC_IS_CLIO) && defined(RPCSPEC_IS_RIPPLED)
#error "rpcspec: RPCSPEC_IS_CLIO and RPCSPEC_IS_RIPPLED are mutually exclusive"
#endif

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Types.hpp>

#include <optional>
#include <tuple>

namespace rpc::spec {

/**
 * @brief Applies a set of validators only when compiled for the Clio server.
 *
 * In rippled builds every member function is a no-op that returns success, so
 * the inner validators are entirely elided. The struct satisfies whichever of
 * `SomeRequirement`, `SomeCheck`, and `SomeModifier` are satisfied by at least
 * one of the inner validators @p Vs.
 *
 * Use the `ifServerClio()` factory alias rather than constructing this directly.
 *
 * @tparam Vs Processor types whose constraints are applied in Clio builds.
 */
template <typename... Vs> struct IfServerClioValidator {
  std::tuple<Vs...> inners;

  /**
   * @brief Constructs the validator with the given set of inner processors.
   *
   * @param vs Inner processors to run in Clio builds.
   */
  consteval explicit IfServerClioValidator(Vs... vs) : inners(vs...) {}

  /**
   * @brief Runs the inner requirements in Clio builds; always succeeds in rippled builds.
   *
   * @param f  Field view for the field under validation.
   * @return   Empty on success; a `rpc::Status` error if any inner requirement fails (Clio only).
   */
  template <SomeFieldView FA>
  [[nodiscard]] MaybeError verify([[maybe_unused]] FA const &f) const
    requires(SomeRequirement<Vs> || ...)
  {
#if RPCSPEC_IS_CLIO
    MaybeError result{};
    std::apply(
        [&](auto const &...vs) {
          auto tryVerify = [&](auto const &v) -> bool {
            if constexpr (SomeRequirement<std::remove_cvref_t<decltype(v)>>) {
              result = v.verify(f);
              return result.has_value();
            }
            return true;
          };
          (tryVerify(vs) && ...);
        },
        inners);
    return result;
#else
    return {};
#endif
  }

  /**
   * @brief Runs the inner checkers in Clio builds; always returns no warning in rippled builds.
   *
   * @param f  Field view for the field under checking.
   * @return   The first warning produced by an inner checker, or `std::nullopt` (Clio only).
   */
  template <SomeFieldView FA>
  [[nodiscard]] std::optional<Warning> check([[maybe_unused]] FA const &f) const
    requires(SomeCheck<Vs> || ...)
  {
#if RPCSPEC_IS_CLIO
    std::optional<Warning> result{};
    std::apply(
        [&](auto const &...vs) {
          auto tryCheck = [&](auto const &v) {
            if constexpr (SomeCheck<std::remove_cvref_t<decltype(v)>>) {
              if (!result)
                result = v.check(f);
            }
          };
          (tryCheck(vs), ...);
        },
        inners);
    return result;
#else
    return std::nullopt;
#endif
  }

  /**
   * @brief Runs the inner modifiers in Clio builds; always succeeds in rippled builds.
   *
   * @param f  Mutable field view for the field under modification.
   * @return   Empty on success; a `rpc::Status` error if any inner modifier fails (Clio only).
   */
  template <SomeFieldView FA>
  [[nodiscard]] MaybeError modify([[maybe_unused]] FA &f) const
    requires(SomeModifier<Vs> || ...)
  {
#if RPCSPEC_IS_CLIO
    MaybeError result{};
    std::apply(
        [&](auto const &...vs) {
          auto tryModify = [&](auto const &v) -> bool {
            if constexpr (SomeModifier<std::remove_cvref_t<decltype(v)>>) {
              result = v.modify(f);
              return result.has_value();
            }
            return true;
          };
          (tryModify(vs) && ...);
        },
        inners);
    return result;
#else
    return {};
#endif
  }
};

/**
 * @brief Applies a set of validators only when compiled for the rippled server.
 *
 * In Clio builds every member function is a no-op that returns success, so
 * the inner validators are entirely elided. The struct satisfies whichever of
 * `SomeRequirement`, `SomeCheck`, and `SomeModifier` are satisfied by at least
 * one of the inner validators @p Vs.
 *
 * Use the `ifServerRippled()` factory alias rather than constructing this directly.
 *
 * @tparam Vs Processor types whose constraints are applied in rippled builds.
 */
template <typename... Vs> struct IfServerRippledValidator {
  std::tuple<Vs...> inners;

  /**
   * @brief Constructs the validator with the given set of inner processors.
   *
   * @param vs Inner processors to run in rippled builds.
   */
  consteval explicit IfServerRippledValidator(Vs... vs) : inners(vs...) {}

  /**
   * @brief Runs the inner requirements in rippled builds; always succeeds in Clio builds.
   *
   * @param f  Field view for the field under validation.
   * @return   Empty on success; a `rpc::Status` error if any inner requirement fails (rippled only).
   */
  template <SomeFieldView FA>
  [[nodiscard]] MaybeError verify([[maybe_unused]] FA const &f) const
    requires(SomeRequirement<Vs> || ...)
  {
#if RPCSPEC_IS_RIPPLED
    MaybeError result{};
    std::apply(
        [&](auto const &...vs) {
          auto tryVerify = [&](auto const &v) -> bool {
            if constexpr (SomeRequirement<std::remove_cvref_t<decltype(v)>>) {
              result = v.verify(f);
              return result.has_value();
            }
            return true;
          };
          (tryVerify(vs) && ...);
        },
        inners);
    return result;
#else
    return {};
#endif
  }

  /**
   * @brief Runs the inner checkers in rippled builds; always returns no warning in Clio builds.
   *
   * @param f  Field view for the field under checking.
   * @return   The first warning produced by an inner checker, or `std::nullopt` (rippled only).
   */
  template <SomeFieldView FA>
  [[nodiscard]] std::optional<Warning> check([[maybe_unused]] FA const &f) const
    requires(SomeCheck<Vs> || ...)
  {
#if RPCSPEC_IS_RIPPLED
    std::optional<Warning> result{};
    std::apply(
        [&](auto const &...vs) {
          auto tryCheck = [&](auto const &v) {
            if constexpr (SomeCheck<std::remove_cvref_t<decltype(v)>>) {
              if (!result)
                result = v.check(f);
            }
          };
          (tryCheck(vs), ...);
        },
        inners);
    return result;
#else
    return std::nullopt;
#endif
  }

  /**
   * @brief Runs the inner modifiers in rippled builds; always succeeds in Clio builds.
   *
   * @param f  Mutable field view for the field under modification.
   * @return   Empty on success; a `rpc::Status` error if any inner modifier fails (rippled only).
   */
  template <SomeFieldView FA>
  [[nodiscard]] MaybeError modify([[maybe_unused]] FA &f) const
    requires(SomeModifier<Vs> || ...)
  {
#if RPCSPEC_IS_RIPPLED
    MaybeError result{};
    std::apply(
        [&](auto const &...vs) {
          auto tryModify = [&](auto const &v) -> bool {
            if constexpr (SomeModifier<std::remove_cvref_t<decltype(v)>>) {
              result = v.modify(f);
              return result.has_value();
            }
            return true;
          };
          (tryModify(vs) && ...);
        },
        inners);
    return result;
#else
    return {};
#endif
  }
};

} // namespace rpc::spec
