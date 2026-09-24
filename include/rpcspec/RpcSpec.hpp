/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/Types.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rpc::spec {

namespace impl {

/**
 * @brief Per-field plan telling the walk which duplicate-key entry wins.
 */
template <std::size_t N>
struct OverridePlan
{
    /**
     * @brief Whether the field at this index is the first use of its key.
     */
    std::array<bool, N> shouldRun;

    /**
     * @brief Index of the last field sharing this key (the one that wins).
     */
    std::array<std::size_t, N> effectiveIdx;
};

/**
 * @brief Work out, for each key, which duplicate entry wins.
 *
 * @param keys The fields' keys, in declaration order.
 * @return The plan for @p keys.
 */
template <std::size_t N>
constexpr OverridePlan<N>
buildOverridePlan(std::array<std::string_view, N> const& keys)
{
    OverridePlan<N> plan{};
    for (auto i = 0uz; i < N; ++i)
    {
        bool isFirst = true;
        for (auto j = 0uz; j < i; ++j)
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
            for (auto j = i + 1; j < N; ++j)
            {
                if (keys[j] == keys[i])
                    last = j;
            }
            plan.effectiveIdx[i] = last;
        }
    }
    return plan;
}

/**
 * @brief Visit each field that survives last-wins key deduplication, in declaration order.
 *
 * Every spec walk — process / check / dump, for both `RpcSpec` and `TypedSpec` — needs the same
 * three steps: collect the keys, build the override plan, then invoke the *effective* field for
 * each surviving key exactly once. Only the per-field action differs, so that is the one thing
 * passed in.
 *
 * @p visit is called as `visit(fields, std::integral_constant<std::size_t, I>{})` for the
 * effective index I of each surviving key. Passing the index as a type keeps the `std::get<I>`
 * inside @p visit a compile-time lookup while the selection stays a runtime table jump — the
 * same shape the hand-written copies had. A @p visit returning `bool` may stop the walk by
 * returning `false`; one returning `void` always runs to completion.
 *
 * @tparam FieldsTuple The spec's field tuple type.
 * @tparam Visit The per-field action.
 * @param fields The spec's fields.
 * @param visit Invoked once per surviving key with the effective field index.
 */
template <typename FieldsTuple, typename Visit, std::size_t... Is>
constexpr void
forEachEffectiveField(FieldsTuple const& fields, Visit visit, std::index_sequence<Is...>)
{
    if constexpr (sizeof...(Is) > 0)
    {
        constexpr auto kN = sizeof...(Is);
        std::array<std::string_view, kN> const keys{std::get<Is>(fields).key...};
        auto const plan = buildOverridePlan(keys);

        using Thunk = bool (*)(FieldsTuple const&, Visit&);
        static constexpr std::array<Thunk, kN> kDispatch{
            +[](FieldsTuple const& fields, Visit& visit) {
                constexpr auto kIdx = std::integral_constant<std::size_t, Is>{};
                if constexpr (std::is_void_v<decltype(visit(fields, kIdx))>)
                {
                    visit(fields, kIdx);
                    return true;
                }
                else
                {
                    return visit(fields, kIdx);
                }
            }...};

        for (auto i = 0uz; i < kN; ++i)
        {
            if (plan.shouldRun[i] and not kDispatch[plan.effectiveIdx[i]](fields, visit))
                return;
        }
    }
}

/**
 * @brief Run every field's processors against @p root.
 *
 * @param fields The spec's fields.
 * @param root The request root to validate.
 * @param seq Index sequence over the fields.
 * @return The first error produced, or empty when all pass.
 */
template <typename FieldsTuple, SomeObjectView Root, std::size_t... Is>
[[nodiscard]] MaybeError
process(FieldsTuple const& fields, Root& root, std::index_sequence<Is...> seq)
{
    MaybeError result{};
    forEachEffectiveField(
        fields,
        [&](FieldsTuple const& fields, auto idx) {
            result = std::get<idx()>(fields).process(root);
            return result.has_value();
        },
        seq);
    return result;
}

/**
 * @brief Collect the warnings of every surviving field.
 *
 * @param fields The spec's fields.
 * @param root The request root to check.
 * @param seq Index sequence over @p fields.
 * @return All warnings produced by the fields' check items.
 */
template <typename FieldsTuple, SomeObjectView Root, std::size_t... Is>
[[nodiscard]] Warnings
check(FieldsTuple const& fields, Root const& root, std::index_sequence<Is...> seq)
{
    Warnings out;
    forEachEffectiveField(
        fields,
        [&](FieldsTuple const& fields, auto idx) {
            auto warnings = std::get<idx()>(fields).check(root);
            out.insert(out.end(), warnings.begin(), warnings.end());
        },
        seq);
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
    /**
     * @brief The spec's fields, as a tuple.
     */
    using FieldsTuple = std::tuple<Fields...>;

    /**
     * @brief The spec's fields, in declaration order.
     */
    FieldsTuple fields;

    /**
     * @brief Construct a @ref RpcSpec.
     *
     * @param fields The fields making up the spec.
     */
    consteval RpcSpec(Fields... fields) : fields{fields...}
    {
    }

    /**
     * @brief Validate @p root, running all field requirements and modifiers.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root Mutable root object view (modifiers may write back into it).
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
     * @param root Const root object view.
     * @return All warnings produced by check items.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        return impl::check(fields, root, std::index_sequence_for<Fields...>{});
    }

    /**
     * @brief `process()` overload accepting a raw document from a backend.
     *
     * @tparam V A value type a backend has bound a view to via `ObjectViewFor`.
     * @param value Mutable value to validate.
     * @return An error on the first failing field; empty on success.
     */
    template <typename V>
        requires(not SomeObjectView<V>) and HasObjectView<V>
    [[nodiscard]] MaybeError
    process(V& value) const
    {
        ObjectViewForT<V> root{value};
        return process(root);
    }

    /**
     * @brief `check()` overload accepting a raw document from a backend.
     *
     * @tparam V A value type a backend has bound a view to via `ObjectViewFor`.
     * @param value Const value to check.
     * @return All warnings produced by check items.
     */
    template <typename V>
        requires(not SomeObjectView<V>) and HasObjectView<V>
    [[nodiscard]] Warnings
    check(V const& value) const
    {
        ObjectViewForT<V> const root{value};
        return check(root);
    }
};

/**
 * @brief Deduction guide for @ref RpcSpec.
 */
template <typename... Fs>
RpcSpec(Fs...) -> RpcSpec<Fs...>;

/**
 * @brief Derive a new `RpcSpec` from @p base by appending extra fields.
 *
 * Fields with duplicate keys follow last-wins override semantics, so @p extra
 * fields can retighten or replace base fields without removing them explicitly.
 *
 * @tparam Existing Field types of the base spec.
 * @tparam Extra Additional field types to append.
 * @param base The spec to extend.
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
 * @param base The spec to extend.
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
