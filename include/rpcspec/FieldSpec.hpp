/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Types.hpp>

#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

namespace rpc::spec {

/**
 * @brief Invoke @p item as a requirement or modifier on @p fieldView, if applicable.
 *
 * Calls `item.verify(fieldView)` for requirements and `item.modify(fieldView)` for modifiers.
 * Returns an empty `MaybeError` for items that are neither (e.g. checks).
 *
 * @tparam Item A field-item type (requirement, modifier, or check).
 * @tparam View A mutable field-view type.
 * @param item The item to dispatch.
 * @param fieldView Mutable view of the field being processed.
 * @return An error if the requirement or modifier fails; empty otherwise.
 */
template <typename Item, typename View>
MaybeError
callIfProcessor(Item const& item, View& fieldView)
{
    if constexpr (SomeRequirement<Item>)
    {
        return item.verify(fieldView);
    }
    else if constexpr (SomeModifier<Item>)
    {
        return item.modify(fieldView);
    }
    else
    {
        return {};
    }
}

/**
 * @brief Invoke @p item as a check on @p fieldView and collect any warning produced.
 *
 * A no-op for items that do not satisfy `SomeCheck`.
 *
 * @tparam Item A field-item type.
 * @tparam View A const field-view type.
 * @param item The item to dispatch.
 * @param fieldView Const view of the field being checked.
 * @param out Warnings collection; a new entry is appended if the check fires.
 */
template <typename Item, typename View>
void
callIfChecker(Item const& item, View const& fieldView, Warnings& out)
{
    if constexpr (SomeCheck<Item>)
    {
        if (auto warning = item.check(fieldView); warning.has_value())
            out.push_back(std::move(*warning));
    }
}

/**
 * @brief Run every requirement/modifier in @p items against @p fieldView, stopping at the first
 * error.
 *
 * The one place the "processors in declaration order, short-circuit on failure" rule is spelled
 * out; `FieldSpec`, `BoundField`, `Section` and `IfType` all defer to it so they cannot drift
 * apart on ordering or short-circuiting.
 *
 * @tparam Items The item tuple's element types.
 * @tparam View A mutable field-view type.
 * @param items The items to run.
 * @param fieldView Mutable view of the field being processed.
 * @return The first error produced, or empty if every item succeeded.
 */
template <typename... Items, typename View>
[[nodiscard]] MaybeError
runProcessors(std::tuple<Items...> const& items, View& fieldView)
{
    MaybeError result{};
    std::apply(
        [&](auto const&... item) {
            (void)((result = callIfProcessor(item, fieldView), result.has_value()) and ...);
        },
        items);
    return result;
}

/**
 * @brief Append the warnings of every check item in @p items to @p out.
 *
 * @tparam Items The item tuple's element types.
 * @tparam View A const field-view type.
 * @param items The items to run.
 * @param fieldView Const view of the field being checked.
 * @param out Warnings collection; each firing check appends one entry.
 */
template <typename... Items, typename View>
void
runChecks(std::tuple<Items...> const& items, View const& fieldView, Warnings& out)
{
    std::apply([&](auto const&... item) { (callIfChecker(item, fieldView, out), ...); }, items);
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
    /**
     * @brief The JSON key this field reads.
     */
    std::string_view key;

    /**
     * @brief The requirements, modifiers and checks attached to this field.
     */
    std::tuple<Items...> items;

    /**
     * @brief Construct a @ref FieldSpec.
     *
     * @param key The JSON key this field reads.
     * @param items The requirements, modifiers and checks to attach.
     */
    consteval FieldSpec(std::string_view key, Items... items) : key{key}, items{items...}
    {
    }

    /**
     * @brief Append a field item, returning a new `FieldSpec` with the extended item list.
     *
     * @tparam Item A requirement, modifier, or check satisfying `SomeFieldItem`.
     * @param item The item to append.
     * @return A new `FieldSpec` with @p item appended after the existing items.
     */
    template <SomeFieldItem Item>
    [[nodiscard]] consteval auto
    operator|(Item item) const
    {
        return appendItem(item, std::index_sequence_for<Items...>{});
    }

    /**
     * @brief Run requirements and modifiers for this field against @p root.
     *
     * Retrieves the child field view for `key`, then executes each requirement
     * and modifier in declaration order, stopping at the first error.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root Mutable root object view.
     * @return An error if any requirement or modifier fails; empty otherwise.
     */
    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    process(Root& root) const
    {
        auto fieldView = root.child(key);
        return runProcessors(items, fieldView);
    }

    /**
     * @brief Collect warnings produced by check items for this field.
     *
     * @tparam Root An object-view type satisfying `SomeObjectView`.
     * @param root Const root object view.
     * @return All warnings emitted by check items for this field.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        auto const fieldView = root.child(key);
        Warnings out;
        runChecks(items, fieldView, out);
        return out;
    }

    /**
     * @brief Run processors for this field starting from a parent field-view.
     *
     * Used by `Section` to validate a nested object field without a full object root.
     *
     * @tparam View A mutable field-view type satisfying `SomeFieldView`.
     * @param parentView Mutable view of the parent field; a child view is derived from it.
     * @return An error if any requirement or modifier fails; empty otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    processNested(View& parentView) const
    {
        auto childView = parentView.child(key);
        return runProcessors(items, childView);
    }

    /**
     * @brief Collect warnings for this field starting from a parent field-view.
     *
     * Used by `Section` for nested object fields.
     *
     * @tparam View A const field-view type satisfying `SomeFieldView`.
     * @param parentView Const view of the parent field; a child view is derived from it.
     * @return All warnings emitted by check items for this field.
     */
    template <SomeFieldView View>
    [[nodiscard]] Warnings
    checkNested(View const& parentView) const
    {
        auto const childView = parentView.child(key);
        Warnings out;
        runChecks(items, childView, out);
        return out;
    }

private:
    // Expanded with an index sequence rather than std::apply and a lambda: the lambda is not an
    // immediate function, so calling a consteval constructor from it relies on P2564, which
    // MSVC does not implement.
    template <typename Item, std::size_t... Is>
    [[nodiscard]] consteval auto
    appendItem(Item item, std::index_sequence<Is...>) const
    {
        return FieldSpec<Items..., Item>{key, std::get<Is>(items)..., item};
    }
};

/**
 * @brief Deduction guide for @ref FieldSpec.
 */
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
 * @param key JSON field name.
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
