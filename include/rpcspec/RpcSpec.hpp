/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/FieldView.hpp>
#include <rpcspec/Types.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

namespace rpc::spec {

namespace impl {

template <std::size_t N>
struct OverridePlan
{
    std::array<bool, N> shouldRun;
    std::array<std::size_t, N> effectiveIdx;
};

template <std::size_t N>
constexpr OverridePlan<N>
buildOverridePlan(std::array<std::string_view, N> const& keys)
{
    OverridePlan<N> plan{};
    for (std::size_t i = 0; i < N; ++i)
    {
        bool isFirst = true;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (keys[j] == keys[i])
            {
                isFirst = false;
                break;
            }
        }
        plan.shouldRun[i] = isFirst;
        if (isFirst)
        {
            std::size_t last = i;
            for (std::size_t j = i + 1; j < N; ++j)
            {
                if (keys[j] == keys[i])
                    last = j;
            }
            plan.effectiveIdx[i] = last;
        }
    }
    return plan;
}

template <typename FieldsTuple, SomeObjectView Root, std::size_t... Is>
[[nodiscard]] MaybeError
process(FieldsTuple const& fields, Root& root, std::index_sequence<Is...>)
{
    if constexpr (sizeof...(Is) == 0)
    {
        return {};
    }
    else
    {
        constexpr auto kN = sizeof...(Is);
        std::array<std::string_view, kN> const keys{std::get<Is>(fields).key...};
        auto const plan = buildOverridePlan(keys);

        using DispatchFn = MaybeError (*)(FieldsTuple const&, Root&);
        static constexpr std::array<DispatchFn, kN> kDISPATCH{
            +[](FieldsTuple const& t, Root& r) -> MaybeError {
                return std::get<Is>(t).process(r);
            }...};

        MaybeError result{};
        for (std::size_t i = 0; i < kN; ++i)
        {
            if (!plan.shouldRun[i])
                continue;
            result = kDISPATCH[plan.effectiveIdx[i]](fields, root);
            if (!result.has_value())
                return result;
        }
        return result;
    }
}

template <typename FieldsTuple, SomeObjectView Root, std::size_t... Is>
[[nodiscard]] Warnings
check(FieldsTuple const& fields, Root const& root, std::index_sequence<Is...>)
{
    Warnings out;
    if constexpr (sizeof...(Is) > 0)
    {
        constexpr auto kN = sizeof...(Is);
        std::array<std::string_view, kN> const keys{std::get<Is>(fields).key...};
        auto const plan = buildOverridePlan(keys);

        using DispatchFn = Warnings (*)(FieldsTuple const&, Root const&);
        static constexpr std::array<DispatchFn, kN> kDISPATCH{
            +[](FieldsTuple const& t, Root const& r) -> Warnings {
                return std::get<Is>(t).check(r);
            }...};

        for (std::size_t i = 0; i < kN; ++i)
        {
            if (!plan.shouldRun[i])
                continue;
            auto w = kDISPATCH[plan.effectiveIdx[i]](fields, root);
            out.insert(out.end(), w.begin(), w.end());
        }
    }
    return out;
}

}  // namespace impl

/**
 * @brief Compile-time RPC request validator composed of typed field specs.
 *
 * Holds a heterogeneous tuple of `FieldSpec` entries (and compatible field
 * types). On `process()` each field's requirements and modifiers run in
 * declaration order using last-wins override semantics for duplicate keys.
 * Build instances via the `spec()` / `extend()` / `field()` factories.
 *
 * @tparam Fields Zero or more field types (e.g. `FieldSpec<...>`).
 */
template <typename... Fields>
struct RpcSpec
{
    using FieldsTuple = std::tuple<Fields...>;
    FieldsTuple fields;

    consteval RpcSpec(Fields... f) : fields{f...}
    {
    }

    /**
     * @brief Validate @p root, running all field requirements and modifiers.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Mutable root object view (modifiers may write back into it).
     * @return An error on the first failing field; empty on success.
     */
    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    process(Root& root) const
    {
        return impl::process(fields, root, std::index_sequence_for<Fields...>{});
    }

    /**
     * @brief Collect all warnings emitted by check items across all fields.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Const root object view.
     * @return All warnings produced by check items.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        return impl::check(fields, root, std::index_sequence_for<Fields...>{});
    }

    /**
     * @brief `process()` overload accepting any value constructible into an `ObjectView`.
     *
     * @tparam V A value type convertible to `ObjectView` (e.g. `boost::json::value`).
     * @param v  Mutable value to validate.
     * @return An error on the first failing field; empty on success.
     */
    template <typename V>
        requires(!SomeObjectView<V>) && std::constructible_from<ObjectView, V&>
    [[nodiscard]] MaybeError
    process(V& v) const
    {
        ObjectView root{v};
        return process(root);
    }

    /**
     * @brief `check()` overload accepting any value constructible into a const `ObjectView`.
     *
     * @tparam V A value type convertible to `ObjectView const`.
     * @param v  Const value to check.
     * @return All warnings produced by check items.
     */
    template <typename V>
        requires(!SomeObjectView<V>) && std::constructible_from<ObjectView, V const&>
    [[nodiscard]] Warnings
    check(V const& v) const
    {
        ObjectView const root{v};
        return check(root);
    }
};

template <typename... Fs>
RpcSpec(Fs...) -> RpcSpec<Fs...>;

/**
 * @brief Derive a new `RpcSpec` from @p base by appending extra fields.
 *
 * Fields with duplicate keys follow last-wins override semantics, so @p extra
 * fields can retighten or replace base fields without removing them explicitly.
 *
 * @tparam Existing Field types of the base spec.
 * @tparam Extra    Additional field types to append.
 * @param base  The spec to extend.
 * @param extra Additional fields appended after the base fields.
 * @return A new `RpcSpec` combining base and extra fields.
 */
template <typename... Existing, typename... Extra>
[[nodiscard]] consteval auto
extend(RpcSpec<Existing...> const& base, Extra... extra)
{
    return std::apply(
        [&](auto const&... existing) { return RpcSpec{existing..., extra...}; }, base.fields);
}

/**
 * @brief Convenience `operator+` alias for `extend(base, extra)`.
 *
 * @tparam Existing Field types of the base spec.
 * @tparam NewItems Item types of the extra `FieldSpec`.
 * @param base  The spec to extend.
 * @param extra A single `FieldSpec` to append.
 * @return A new `RpcSpec` with @p extra appended.
 */
template <typename... Existing, typename... NewItems>
[[nodiscard]] consteval auto
operator+(RpcSpec<Existing...> const& base, FieldSpec<NewItems...> extra)
{
    return extend(base, extra);
}

}  // namespace rpc::spec
