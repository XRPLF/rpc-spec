/** @file */
#pragma once
// Compile-time server-conditional validator wrappers for the rpcspec DSL.
//
// Exactly one of RPCSPEC_IS_CLIO or RPCSPEC_IS_RIPPLED must be defined by the
// consuming project's build system (set automatically via each project's conanfile).
//
// ifServerClio(v...)    — applies validators only in Clio builds
// ifServerRippled(v...) — applies validators only in rippled builds
//
// Each wrapper satisfies the union of the inner validators' concepts:
//   SomeRequirement<IfServerClioValidator<Vs...>>  iff (SomeRequirement<Vs> || ...)
//   SomeCheck<IfServerClioValidator<Vs...>>        iff (SomeCheck<Vs> || ...)
//   SomeModifier<IfServerClioValidator<Vs...>>     iff (SomeModifier<Vs> || ...)
//
// Multiple validators may be passed; they are applied in order:
//   field("full", ifServerClio(notSupportedIf(true), deprecated))
//   field("diff", ifServerClio(type<bool>))

#if !defined(RPCSPEC_IS_CLIO) && !defined(RPCSPEC_IS_RIPPLED)
#error "rpcspec: must define RPCSPEC_IS_CLIO=1 or RPCSPEC_IS_RIPPLED=1 (set in your project's conanfile)"
#endif
#if defined(RPCSPEC_IS_CLIO) && defined(RPCSPEC_IS_RIPPLED)
#error "rpcspec: RPCSPEC_IS_CLIO and RPCSPEC_IS_RIPPLED are mutually exclusive"
#endif

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Types.hpp>

#include <optional>
#include <tuple>

namespace rpc::spec {

template <typename... Vs>
struct IfServerClioValidator {
    std::tuple<Vs...> inners;

    consteval explicit IfServerClioValidator(Vs... vs) : inners(vs...)
    {
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify([[maybe_unused]] FA const& f) const
        requires(SomeRequirement<Vs> || ...)
    {
#if RPCSPEC_IS_CLIO
        MaybeError result{};
        std::apply(
            [&](auto const&... vs) {
                auto tryVerify = [&](auto const& v) -> bool {
                    if constexpr (SomeRequirement<std::remove_cvref_t<decltype(v)>>) {
                        result = v.verify(f);
                        return result.has_value();
                    }
                    return true;
                };
                (tryVerify(vs) && ...);
            },
            inners
        );
        return result;
#else
        return {};
#endif
    }

    template <SomeFieldView FA>
    [[nodiscard]] std::optional<Warning>
    check([[maybe_unused]] FA const& f) const
        requires(SomeCheck<Vs> || ...)
    {
#if RPCSPEC_IS_CLIO
        std::optional<Warning> result{};
        std::apply(
            [&](auto const&... vs) {
                auto tryCheck = [&](auto const& v) {
                    if constexpr (SomeCheck<std::remove_cvref_t<decltype(v)>>) {
                        if (!result)
                            result = v.check(f);
                    }
                };
                (tryCheck(vs), ...);
            },
            inners
        );
        return result;
#else
        return std::nullopt;
#endif
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    modify([[maybe_unused]] FA& f) const
        requires(SomeModifier<Vs> || ...)
    {
#if RPCSPEC_IS_CLIO
        MaybeError result{};
        std::apply(
            [&](auto const&... vs) {
                auto tryModify = [&](auto const& v) -> bool {
                    if constexpr (SomeModifier<std::remove_cvref_t<decltype(v)>>) {
                        result = v.modify(f);
                        return result.has_value();
                    }
                    return true;
                };
                (tryModify(vs) && ...);
            },
            inners
        );
        return result;
#else
        return {};
#endif
    }
};

template <typename... Vs>
struct IfServerRippledValidator {
    std::tuple<Vs...> inners;

    consteval explicit IfServerRippledValidator(Vs... vs) : inners(vs...)
    {
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify([[maybe_unused]] FA const& f) const
        requires(SomeRequirement<Vs> || ...)
    {
#if RPCSPEC_IS_RIPPLED
        MaybeError result{};
        std::apply(
            [&](auto const&... vs) {
                auto tryVerify = [&](auto const& v) -> bool {
                    if constexpr (SomeRequirement<std::remove_cvref_t<decltype(v)>>) {
                        result = v.verify(f);
                        return result.has_value();
                    }
                    return true;
                };
                (tryVerify(vs) && ...);
            },
            inners
        );
        return result;
#else
        return {};
#endif
    }

    template <SomeFieldView FA>
    [[nodiscard]] std::optional<Warning>
    check([[maybe_unused]] FA const& f) const
        requires(SomeCheck<Vs> || ...)
    {
#if RPCSPEC_IS_RIPPLED
        std::optional<Warning> result{};
        std::apply(
            [&](auto const&... vs) {
                auto tryCheck = [&](auto const& v) {
                    if constexpr (SomeCheck<std::remove_cvref_t<decltype(v)>>) {
                        if (!result)
                            result = v.check(f);
                    }
                };
                (tryCheck(vs), ...);
            },
            inners
        );
        return result;
#else
        return std::nullopt;
#endif
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    modify([[maybe_unused]] FA& f) const
        requires(SomeModifier<Vs> || ...)
    {
#if RPCSPEC_IS_RIPPLED
        MaybeError result{};
        std::apply(
            [&](auto const&... vs) {
                auto tryModify = [&](auto const& v) -> bool {
                    if constexpr (SomeModifier<std::remove_cvref_t<decltype(v)>>) {
                        result = v.modify(f);
                        return result.has_value();
                    }
                    return true;
                };
                (tryModify(vs) && ...);
            },
            inners
        );
        return result;
#else
        return {};
#endif
    }
};

}  // namespace rpc::spec
