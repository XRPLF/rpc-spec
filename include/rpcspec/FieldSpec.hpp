/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Types.hpp>

#include <string_view>
#include <tuple>

namespace rpc::spec {

/**
 * @brief Invoke @p item as a requirement or modifier on @p fa, if applicable.
 *
 * Calls `item.verify(fa)` for requirements and `item.modify(fa)` for modifiers.
 * Returns an empty `MaybeError` for items that are neither (e.g. checks).
 *
 * @tparam Item A field-item type (requirement, modifier, or check).
 * @tparam FA   A mutable field-accessor / field-view type.
 * @param item  The item to dispatch.
 * @param fa    Mutable view of the field being processed.
 * @return An error if the requirement or modifier fails; empty otherwise.
 */
template <typename Item, typename FA>
MaybeError
callIfProcessor(Item const& item, FA& fa)
{
    if constexpr (SomeRequirement<Item>)
    {
        return item.verify(fa);
    }
    else if constexpr (SomeModifier<Item>)
    {
        return item.modify(fa);
    }
    else
    {
        return {};
    }
}

/**
 * @brief Invoke @p item as a check on @p fa and collect any warning produced.
 *
 * A no-op for items that do not satisfy `SomeCheck`.
 *
 * @tparam Item A field-item type.
 * @tparam FA   A const field-accessor / field-view type.
 * @param item  The item to dispatch.
 * @param fa    Const view of the field being checked.
 * @param out   Warnings collection; a new entry is appended if the check fires.
 */
template <typename Item, typename FA>
void
callIfChecker(Item const& item, FA const& fa, Warnings& out)
{
    if constexpr (SomeCheck<Item>)
    {
        if (auto w = item.check(fa))
            out.push_back(std::move(*w));
    }
}

/**
 * @brief A single named field in an @ref RpcSpec, paired with its validation items.
 *
 * Holds a JSON key and an ordered tuple of requirements, modifiers, and checks.
 * Items are executed left-to-right during `process()` and `check()`; a failing
 * requirement short-circuits further processing for that field.
 * Build instances via the `field()` factory; chain items with `operator|`.
 *
 * @tparam Items Zero or more field-item types (requirements, modifiers, checks).
 */
template <SomeFieldItem... Items>
struct FieldSpec
{
    std::string_view key;
    std::tuple<Items...> items;

    consteval FieldSpec(std::string_view k, Items... i) : key{k}, items{i...}
    {
    }

    /**
     * @brief Append a field item, returning a new `FieldSpec` with the extended item list.
     *
     * @tparam Item A requirement, modifier, or check satisfying `SomeFieldItem`.
     * @param item  The item to append.
     * @return A new `FieldSpec` with @p item appended after the existing items.
     */
    template <SomeFieldItem Item>
    [[nodiscard]] consteval auto
    operator|(Item item) const
    {
        return std::apply(
            [&](auto const&... existing) {
                return FieldSpec<Items..., Item>{key, existing..., item};
            },
            items);
    }

    /**
     * @brief Run requirements and modifiers for this field against @p root.
     *
     * Retrieves the child field view for `key`, then executes each requirement
     * and modifier in declaration order, stopping at the first error.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Mutable root object view.
     * @return An error if any requirement or modifier fails; empty otherwise.
     */
    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    process(Root& root) const
    {
        auto fa = root.child(key);
        MaybeError result{};
        std::apply(
            [&](auto const&... item) {
                (void)((result = callIfProcessor(item, fa), result.has_value()) && ...);
            },
            items);
        return result;
    }

    /**
     * @brief Collect warnings produced by check items for this field.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root  Const root object view.
     * @return All warnings emitted by check items for this field.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        auto fa = root.child(key);
        Warnings out;
        std::apply([&](auto const&... item) { (callIfChecker(item, fa, out), ...); }, items);
        return out;
    }

    /**
     * @brief Run processors for this field starting from a parent field-view.
     *
     * Used by `Section` to validate a nested object field without a full object root.
     *
     * @tparam FA   A mutable field-view type satisfying `SomeFieldView`.
     * @param parentFa  Mutable view of the parent field; a child view is derived from it.
     * @return An error if any requirement or modifier fails; empty otherwise.
     */
    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    processNested(FA& parentFa) const
    {
        auto childFa = parentFa.child(key);
        MaybeError result{};
        std::apply(
            [&](auto const&... item) {
                (void)((result = callIfProcessor(item, childFa), result.has_value()) && ...);
            },
            items);
        return result;
    }

    /**
     * @brief Collect warnings for this field starting from a parent field-view.
     *
     * Used by `Section` for nested object fields.
     *
     * @tparam FA   A const field-view type satisfying `SomeFieldView`.
     * @param parentFa  Const view of the parent field; a child view is derived from it.
     * @return All warnings emitted by check items for this field.
     */
    template <SomeFieldView FA>
    [[nodiscard]] Warnings
    checkNested(FA const& parentFa) const
    {
        auto const childFa = parentFa.child(key);
        Warnings out;
        std::apply([&](auto const&... item) { (callIfChecker(item, childFa, out), ...); }, items);
        return out;
    }
};

template <SomeFieldItem... Is>
FieldSpec(std::string_view, Is...) -> FieldSpec<Is...>;

/**
 * @brief Create an empty `FieldSpec` for @p key; attach items via `operator|`.
 *
 * @param key JSON field name.
 * @return A `FieldSpec` with no items; chain modifiers/checks with `operator|`.
 */
consteval auto
field(std::string_view key)
{
    return FieldSpec<>{key};
}

/**
 * @brief Create a `FieldSpec` for @p key with inline items.
 *
 * Equivalent to `field(key) | item0 | item1 | ...` but in a single call.
 *
 * @tparam Items Zero or more field-item types (requirements, modifiers, checks).
 * @param key   JSON field name.
 * @param items Field items in execution order.
 * @return A fully constructed `FieldSpec`.
 */
template <SomeFieldItem... Items>
consteval auto
field(std::string_view key, Items... items)
{
    return FieldSpec{key, items...};
}

}  // namespace rpc::spec
