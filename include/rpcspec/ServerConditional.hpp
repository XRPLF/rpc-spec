/** @file */
#pragma once
// Compile-time server-conditional validator wrappers for the rpcspec DSL.
//
// Exactly one of RPCSPEC_IS_CLIO or RPCSPEC_IS_XRPLD must be defined by the
// consuming project's build system (set automatically via each project's
// conanfile).
//
// ifServerClio(v...)  — applies validators only in Clio builds
// ifServerXrpld(v...) — applies validators only in xrpld builds
//
// Each wrapper satisfies the union of the inner validators' concepts:
//   SomeRequirement<IfServerClioValidator<Vs...>> iff (SomeRequirement<Vs> || ...)
//   SomeCheck<IfServerClioValidator<Vs...>>       iff (SomeCheck<Vs> || ...)
//   SomeModifier<IfServerClioValidator<Vs...>>    iff (SomeModifier<Vs> || ...)
//
// Multiple validators may be passed; they are applied in order:
//   field("full", ifServerClio(notSupportedIf(true), deprecated))
//   field("diff", ifServerClio(type<bool>))

#if !defined(RPCSPEC_IS_CLIO) && !defined(RPCSPEC_IS_XRPLD)
#error \
    "rpcspec: must define RPCSPEC_IS_CLIO=1 or RPCSPEC_IS_XRPLD=1 (set in your project's conanfile)"
#endif
#if defined(RPCSPEC_IS_CLIO) && defined(RPCSPEC_IS_XRPLD)
#error "rpcspec: RPCSPEC_IS_CLIO and RPCSPEC_IS_XRPLD are mutually exclusive"
#endif

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Types.hpp>

#include <optional>
#include <tuple>
#include <type_traits>

namespace rpc::spec {

// The server macros are lifted to constexpr booleans once, here, so the rest of the DSL can
// branch with `if constexpr` instead of scattering #ifdefs through template bodies.

/**
 * @brief True in a Clio build. The checks above guarantee exactly one of the two is true.
 */
inline constexpr bool kIsClioBuild =
#if defined(RPCSPEC_IS_CLIO)
    true;
#else
    false;
#endif

/**
 * @brief True in an xrpld build.
 */
inline constexpr bool kIsXrpldBuild = not kIsClioBuild;

/**
 * @brief Applies a set of validators only when @p Active — i.e. only in the matching build.
 *
 * When @p Active is false every member function is a no-op returning success, so the inner
 * validators are entirely elided. The struct satisfies whichever of `SomeRequirement`,
 * `SomeCheck`, and `SomeModifier` are satisfied by at least one of the inner validators @p Vs,
 * regardless of @p Active — the wrapper must present the same interface in both builds so a
 * spec literal compiles identically for either server.
 *
 * Use the `ifServerClio()` / `ifServerXrpld()` factories rather than constructing this directly.
 *
 * @tparam Active Whether the inner validators run in this build.
 * @tparam Vs Processor types to apply when @p Active.
 */
template <bool Active, typename... Vs>
struct ServerConditionalValidator
{
    /**
     * @brief Processors run only in the matching server build.
     */
    std::tuple<Vs...> inners;

    /**
     * @brief Constructs the validator with the given set of inner processors.
     *
     * @param vs Inner processors to run when @p Active.
     */
    consteval explicit ServerConditionalValidator(Vs... vs) : inners(vs...)
    {
    }

    /**
     * @brief Runs the inner requirements in the matching build; always succeeds otherwise.
     *
     * @param fieldView Field view for the field under validation.
     * @return   Empty on success; a `rpc::Status` error if any inner requirement fails.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify([[maybe_unused]] View const& fieldView) const
        requires(SomeRequirement<Vs> or ...)
    {
        MaybeError result{};
        if constexpr (Active)
        {
            std::apply(
                [&](auto const&... vs) {
                    auto tryVerify = [&](auto const& validator) -> bool {
                        if constexpr (SomeRequirement<std::remove_cvref_t<decltype(validator)>>)
                        {
                            result = validator.verify(fieldView);
                            return result.has_value();
                        }
                        return true;
                    };
                    (tryVerify(vs) and ...);
                },
                inners);
        }
        return result;
    }

    /**
     * @brief Runs the inner checkers in the matching build; always returns no warning otherwise.
     *
     * @param fieldView Field view for the field under checking.
     * @return   The first warning produced by an inner checker, or `std::nullopt`.
     */
    template <SomeFieldView View>
    [[nodiscard]] std::optional<Warning>
    check([[maybe_unused]] View const& fieldView) const
        requires(SomeCheck<Vs> or ...)
    {
        std::optional<Warning> result{};
        if constexpr (Active)
        {
            std::apply(
                [&](auto const&... vs) {
                    auto tryCheck = [&](auto const& validator) {
                        if constexpr (SomeCheck<std::remove_cvref_t<decltype(validator)>>)
                        {
                            if (not result.has_value())
                                result = validator.check(fieldView);
                        }
                    };
                    (tryCheck(vs), ...);
                },
                inners);
        }
        return result;
    }

    /**
     * @brief Runs the inner modifiers in the matching build; always succeeds otherwise.
     *
     * @param fieldView Mutable field view for the field under modification.
     * @return   Empty on success; a `rpc::Status` error if any inner modifier fails.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    modify([[maybe_unused]] View& fieldView) const
        requires(SomeModifier<Vs> or ...)
    {
        MaybeError result{};
        if constexpr (Active)
        {
            std::apply(
                [&](auto const&... vs) {
                    auto tryModify = [&](auto const& validator) -> bool {
                        if constexpr (SomeModifier<std::remove_cvref_t<decltype(validator)>>)
                        {
                            result = validator.modify(fieldView);
                            return result.has_value();
                        }
                        return true;
                    };
                    (tryModify(vs) and ...);
                },
                inners);
        }
        return result;
    }
};

/**
 * @brief Applies a set of validators only when compiled for the Clio server.
 */
template <typename... Vs>
using IfServerClioValidator = ServerConditionalValidator<kIsClioBuild, Vs...>;

/**
 * @brief Applies a set of validators only when compiled for the xrpld server.
 */
template <typename... Vs>
using IfServerXrpldValidator = ServerConditionalValidator<kIsXrpldBuild, Vs...>;

}  // namespace rpc::spec
