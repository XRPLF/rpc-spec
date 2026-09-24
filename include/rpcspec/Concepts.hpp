/** @file */
#pragma once

#include <rpcspec/Types.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace rpc::spec {

/**
 * @brief Customization point mapping a backend's JSON value type to its object view.
 *
 * The spec library names no JSON type. A backend specialises this for its value type
 * (see `rpcspec/backends/BoostJson.hpp`), which is what lets the convenience overloads
 * accept a raw document — `spec.parse(jsonValue)` — instead of making every caller wrap
 * it in a view by hand.
 *
 * @tparam Value The backend's JSON value type.
 */
template <typename Value>
struct ObjectViewFor;

/**
 * @brief True when @p Value has a view bound to it by a backend.
 */
template <typename Value>
concept HasObjectView = requires { typename ObjectViewFor<std::remove_const_t<Value>>::Type; };

/**
 * @brief The object view bound to @p Value.
 */
template <typename Value>
using ObjectViewForT = typename ObjectViewFor<std::remove_const_t<Value>>::Type;

/**
 * @brief Non-owning view of a single resolved field within a JSON document.
 *
 * Implemented by a backend type (see include/rpcspec/backends). Validators and
 * modifiers receive instances of any such type through a template parameter,
 * so they never depend on a concrete JSON library.
 */
template <typename T>
concept SomeFieldView = requires(T view, T const constView) {
    { constView.key() } -> std::convertible_to<std::string_view>;
    { constView.present() } -> std::convertible_to<bool>;
    { constView.isInt64() } -> std::convertible_to<bool>;
    { constView.asInt64() } -> std::convertible_to<int64_t>;
    { constView.isUint32() } -> std::convertible_to<bool>;
    { constView.asUint32() } -> std::convertible_to<uint32_t>;
    { constView.isBool() } -> std::convertible_to<bool>;
    { constView.asBool() } -> std::convertible_to<bool>;
    { constView.isString() } -> std::convertible_to<bool>;
    { constView.asString() } -> std::convertible_to<std::string_view>;
    { constView.isDouble() } -> std::convertible_to<bool>;
    { constView.asDouble() } -> std::convertible_to<double>;
    { constView.isObject() } -> std::convertible_to<bool>;
    { constView.isArray() } -> std::convertible_to<bool>;
    { constView.arraySize() } -> std::convertible_to<std::size_t>;
    { constView.objectSize() } -> std::convertible_to<std::size_t>;
    { constView.template is<JsonObject>() } -> std::convertible_to<bool>;
    { constView.template is<JsonArray>() } -> std::convertible_to<bool>;
    { constView.template is<int64_t>() } -> std::convertible_to<bool>;
    { constView.template is<uint32_t>() } -> std::convertible_to<bool>;
    { constView.template is<bool>() } -> std::convertible_to<bool>;
    { constView.template is<std::string>() } -> std::convertible_to<bool>;
    { constView.template is<double>() } -> std::convertible_to<bool>;
    { constView.child(std::string_view{}) } -> std::same_as<T>;
    { constView.element(std::size_t{}) } -> std::same_as<T>;
    { view.set(int64_t{}) };
    { view.set(uint32_t{}) };
    { view.set(std::string_view{}) };
    { view.set(bool{}) };
    { view.set(double{}) };
};

namespace detail {

// Archetype satisfying SomeFieldView. Used as the witness type for
// validator/modifier/checker concepts so they aren't coupled to any backend.
// Never instantiated; declarations only.
/**
 * @brief Declaration-only archetype used to witness the validator concepts without a backend.
 */
struct FieldViewArchetype
{
    /**
     * @brief The field's key.
     *
     * @return The JSON key this view was resolved from.
     */
    [[nodiscard]] std::string_view
    key() const noexcept;

    /**
     * @brief Whether the field was present in the request.
     *
     * @return true when the field exists; false otherwise.
     */
    [[nodiscard]] bool
    present() const noexcept;

    /**
     * @brief Whether the field holds a signed 64-bit integer.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isInt64() const noexcept;

    /**
     * @brief Read the field as a signed 64-bit integer.
     *
     * @return The value; only valid when isInt64() is true.
     */
    [[nodiscard]] int64_t
    asInt64() const;

    /**
     * @brief Whether the field holds a value representable as an unsigned 32-bit integer.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isUint32() const noexcept;

    /**
     * @brief Read the field as an unsigned 32-bit integer.
     *
     * @return The value; only valid when isUint32() is true.
     */
    [[nodiscard]] uint32_t
    asUint32() const;

    /**
     * @brief Whether the field holds a boolean.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isBool() const noexcept;

    /**
     * @brief Read the field as a boolean.
     *
     * @return The value; only valid when isBool() is true.
     */
    [[nodiscard]] bool
    asBool() const;

    /**
     * @brief Whether the field holds a string.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isString() const noexcept;

    /**
     * @brief Read the field as a string.
     *
     * @return The value; only valid when isString() is true.
     */
    [[nodiscard]] std::string_view
    asString() const;

    /**
     * @brief Whether the field holds a double.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isDouble() const noexcept;

    /**
     * @brief Read the field as a double.
     *
     * @return The value; only valid when isDouble() is true.
     */
    [[nodiscard]] double
    asDouble() const;

    /**
     * @brief Whether the value is a JSON object.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isObject() const noexcept;

    /**
     * @brief Whether the value is a JSON array.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isArray() const noexcept;

    /**
     * @brief Number of elements, for an array value.
     *
     * @return The element count, or 0 when this is not an array.
     */
    [[nodiscard]] std::size_t
    arraySize() const noexcept;

    /**
     * @brief Number of members, for an object value.
     *
     * @return The member count, or 0 when this is not an object.
     */
    [[nodiscard]] std::size_t
    objectSize() const noexcept;

    /**
     * @brief Whether the field holds a value of type @p T.
     *
     * @tparam T The JSON value type to test for.
     * @return true when the field is present and holds a @p T; false otherwise.
     */
    template <typename T>
    [[nodiscard]] bool
    is() const noexcept;

    /**
     * @brief Resolve a named sub-field.
     *
     * @param key The sub-field name.
     * @return A view of that sub-field, possibly absent.
     */
    [[nodiscard]] FieldViewArchetype
    child(std::string_view key) const noexcept;

    /**
     * @brief Resolve an array element.
     *
     * @param idx Zero-based element index.
     * @return A view of that element, possibly absent.
     */
    [[nodiscard]] FieldViewArchetype
    element(std::size_t idx) const noexcept;

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(int64_t value);

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(uint32_t value);

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(std::string_view value);

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(bool value);

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(double value);
};

}  // namespace detail

static_assert(SomeFieldView<detail::FieldViewArchetype>);

/**
 * @brief Non-owning view of the document root, which is always an object/dict.
 *
 * Distinct from SomeFieldView: the root has no name, is always present, cannot
 * be set, and is only used by RpcSpec/FieldSpec to navigate into named fields via
 * child(). Keeping the type distinct prevents passing a keyless field view into
 * validators.
 */
template <typename T>
concept SomeObjectView = requires(T const constRoot, T& root) {
    { constRoot.isObject() } -> std::convertible_to<bool>;
    { constRoot.isArray() } -> std::convertible_to<bool>;
    { root.child(std::string_view{}) } -> SomeFieldView;
    { constRoot.child(std::string_view{}) } -> SomeFieldView;
};

namespace detail {

// Archetype satisfying SomeObjectView. Used as the witness type for spec-level
// concepts so they aren't coupled to any backend. Never instantiated.
/**
 * @brief Declaration-only archetype used to witness the spec-level concepts without a backend.
 */
struct ObjectViewArchetype
{
    /**
     * @brief Whether the value is a JSON object.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isObject() const noexcept;

    /**
     * @brief Whether the value is a JSON array.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isArray() const noexcept;

    /**
     * @brief Resolve a named sub-field.
     *
     * @param key The sub-field name.
     * @return A view of that sub-field, possibly absent.
     */
    [[nodiscard]] FieldViewArchetype
    child(std::string_view key) noexcept;

    /**
     * @brief Resolve a named sub-field.
     *
     * @param key The sub-field name.
     * @return A view of that sub-field, possibly absent.
     */
    [[nodiscard]] FieldViewArchetype
    child(std::string_view key) const noexcept;
};

}  // namespace detail

static_assert(SomeObjectView<detail::ObjectViewArchetype>);

/**
 * @brief A type that can validate a field without modifying it.
 *
 * Must expose a `verify(View const&) -> MaybeError` method. Validators that
 * return an error abort further processing of the field.
 *
 * @tparam T The candidate type to check.
 */
template <typename T>
concept SomeRequirement = requires(T const item, detail::FieldViewArchetype const& fieldView) {
    { item.verify(fieldView) } -> std::same_as<MaybeError>;
};

/**
 * @brief A type that can modify a field in place during processing.
 *
 * Must expose a `modify(View&) -> MaybeError` method. Modifiers receive a
 * mutable field view and may rewrite the field value (e.g. toLower, clamp).
 *
 * @tparam T The candidate type to check.
 */
template <typename T>
concept SomeModifier = requires(T const item, detail::FieldViewArchetype& fieldView) {
    { item.modify(fieldView) } -> std::same_as<MaybeError>;
};

/**
 * @brief A type that can emit non-blocking warnings for a field.
 *
 * Must expose a `check(View const&) -> std::optional<Warning>` method.
 * Checkers never fail validation; they only advise (e.g. deprecation notices).
 *
 * @tparam T The candidate type to check.
 */
template <typename T>
concept SomeCheck = requires(T const item, detail::FieldViewArchetype const& fieldView) {
    { item.check(fieldView) } -> std::same_as<std::optional<Warning>>;
};

/**
 * @brief A type that is either a SomeRequirement or a SomeModifier.
 *
 * @tparam T The candidate type to check.
 */
template <typename T>
concept SomeProcessor = SomeRequirement<T> or SomeModifier<T>;

/**
 * @brief A pure default marker: carries the value a bound field receives when absent.
 *
 * Neither a requirement, modifier, nor check, so it is a no-op during process()/check().
 * `BoundField::parseInto` detects it and assigns `value` to the bound member when the
 * field is omitted from the request (see Typed.hpp / `defaultTo`).
 *
 * @tparam T The candidate type to check.
 */
template <typename T>
concept SomeDefault = requires {
    requires std::same_as<std::remove_cv_t<decltype(T::kIsDefault)>, bool>;
    requires T::kIsDefault;
    typename T::ValueType;
};

/**
 * @brief A type that is a SomeProcessor, SomeCheck, or SomeDefault — any item attachable to a
 * FieldSpec.
 *
 * @tparam T The candidate type to check.
 */
template <typename T>
concept SomeFieldItem = SomeProcessor<T> or SomeCheck<T> or SomeDefault<T>;

}  // namespace rpc::spec
