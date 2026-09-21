/** @file */
#pragma once

#include <boost/json/string.hpp>
#include <boost/json/value.hpp>

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace rpc::spec {

/**
 * @brief Non-owning view of a single resolved field, usable by validators and
 * modifiers.
 *
 * Backends provide a concrete type satisfying SomeFieldView (defined in
 * Concepts.hpp) and a SomeObjectView type whose child() method returns it.
 * FieldSpec obtains the View via the root's child(key); validators never see the
 * raw JSON type.
 *
 * Two constructors carry const-correctness through:
 *   - mutable ctor (from value&):       both readValue_ and writeValue_ are set
 *   - const ctor   (from value const&): only readValue_ is set; set() is
 * unreachable during check() since callIfChecker passes View const& which
 * prevents non-const calls.
 */
class BoostJsonFieldView
{
    boost::json::value const* readValue_;  // nullptr when field is absent
    boost::json::value* writeValue_;       // nullptr when constructed from const

    std::string_view key_;

public:
    /**
     * @brief Construct a mutable view: both read and write access are enabled.
     *
     * @param value Pointer to the JSON value (may be null when the field is absent).
     * @param key The field key, used in error messages and as the view's identity.
     */
    BoostJsonFieldView(boost::json::value* value, std::string_view key) noexcept
        : readValue_{value}, writeValue_{value}, key_{key}
    {
    }

    /**
     * @brief Construct a read-only view: write access is disabled.
     *
     * @param value Pointer to the const JSON value (may be null when the field is absent).
     * @param key The field key, used in error messages and as the view's identity.
     */
    BoostJsonFieldView(boost::json::value const* value, std::string_view key) noexcept
        : readValue_{value}, writeValue_{nullptr}, key_{key}
    {
    }

    /**
     * @brief An absent view for @p k, derived from a mutable parent.
     *
     * A bare `nullptr` is ambiguous between the two constructors, so absent views need a cast
     * to pick one. These two factories carry that cast once instead of at every return site.
     *
     * @param key The key the absent view reports.
     * @return An absent view whose (unreachable) write path matches a mutable parent.
     */
    [[nodiscard]] static BoostJsonFieldView
    absentMutable(std::string_view key) noexcept
    {
        return {static_cast<boost::json::value*>(nullptr), key};
    }

    /**
     * @brief An absent read-only view for @p k.
     *
     * @param key The key the absent view reports.
     * @return An absent, read-only view.
     */
    [[nodiscard]] static BoostJsonFieldView
    absentConst(std::string_view key) noexcept
    {
        return {static_cast<boost::json::value const*>(nullptr), key};
    }

    /**
     * @brief The field's key.
     *
     * @return The JSON key this view was resolved from.
     */
    [[nodiscard]] std::string_view
    key() const noexcept
    {
        return key_;
    }

    /**
     * @brief Whether the field was present in the request.
     *
     * @return true when the field exists; false otherwise.
     */
    [[nodiscard]] bool
    present() const noexcept
    {
        return readValue_ != nullptr;
    }

    /**
     * @brief Whether the field holds a signed 64-bit integer.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isInt64() const noexcept
    {
        return readValue_ != nullptr and
            (readValue_->is_int64() or
             (readValue_->is_uint64() and
              readValue_->as_uint64() <=
                  static_cast<uint64_t>(std::numeric_limits<int64_t>::max())));
    }

    /**
     * @brief Read the field as a signed 64-bit integer.
     *
     * @return The value; only valid when isInt64() is true.
     */
    [[nodiscard]] int64_t
    asInt64() const
    {
        return readValue_->is_int64() ? readValue_->as_int64()
                                      : static_cast<int64_t>(readValue_->as_uint64());
    }

    /**
     * @brief Whether the field holds a value representable as an unsigned 32-bit integer.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isUint32() const noexcept
    {
        if (readValue_ == nullptr)
            return false;
        if (readValue_->is_uint64())
            return readValue_->as_uint64() <= std::numeric_limits<uint32_t>::max();
        if (readValue_->is_int64())
        {
            auto const value = readValue_->as_int64();
            return value >= 0 and
                value <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
        }
        return false;
    }

    /**
     * @brief Read the field as an unsigned 32-bit integer.
     *
     * @return The value; only valid when isUint32() is true.
     */
    [[nodiscard]] uint32_t
    asUint32() const
    {
        return readValue_->is_uint64() ? static_cast<uint32_t>(readValue_->as_uint64())
                                       : static_cast<uint32_t>(readValue_->as_int64());
    }

    /**
     * @brief Whether the field holds a boolean.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isBool() const noexcept
    {
        return readValue_ != nullptr and readValue_->is_bool();
    }

    /**
     * @brief Read the field as a boolean.
     *
     * @return The value; only valid when isBool() is true.
     */
    [[nodiscard]] bool
    asBool() const
    {
        return readValue_->as_bool();
    }

    /**
     * @brief Whether the field holds a string.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isString() const noexcept
    {
        return readValue_ != nullptr and readValue_->is_string();
    }

    /**
     * @brief Read the field as a string.
     *
     * @return The value; only valid when isString() is true.
     */
    [[nodiscard]] std::string_view
    asString() const
    {
        return readValue_->as_string();
    }

    /**
     * @brief Whether the field holds a double.
     *
     * @return true when it does; false otherwise.
     */
    [[nodiscard]] bool
    isDouble() const noexcept
    {
        return readValue_ != nullptr and readValue_->is_double();
    }

    /**
     * @brief Read the field as a double.
     *
     * @return The value; only valid when isDouble() is true.
     */
    [[nodiscard]] double
    asDouble() const
    {
        return readValue_->as_double();
    }

    /**
     * @brief Whether the value is a JSON object.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isObject() const noexcept
    {
        return readValue_ != nullptr and readValue_->is_object();
    }

    /**
     * @brief Whether the value is a JSON array.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isArray() const noexcept
    {
        return readValue_ != nullptr and readValue_->is_array();
    }

    /**
     * @brief Number of elements, for an array value.
     *
     * @return The element count, or 0 when this is not an array.
     */
    [[nodiscard]] std::size_t
    arraySize() const noexcept
    {
        if (readValue_ == nullptr or not readValue_->is_array())
            return 0;
        return readValue_->as_array().size();
    }

    /**
     * @brief Number of members, for an object value.
     *
     * @return The member count, or 0 when this is not an object.
     */
    [[nodiscard]] std::size_t
    objectSize() const noexcept
    {
        if (readValue_ == nullptr or not readValue_->is_object())
            return 0;
        return readValue_->as_object().size();
    }

    /**
     * @brief Return a view for a named sub-field within this field (must be an object).
     *
     * If this field is absent, not an object, or `childKey` is not found, returns
     * an absent view. Mutable access propagates from the parent: if this view was
     * constructed from a mutable value, the child view is also mutable.
     *
     * @param childKey The key of the sub-field to look up.
     * @return A BoostJsonFieldView for the named child, possibly absent.
     */
    [[nodiscard]] BoostJsonFieldView
    child(std::string_view childKey) const noexcept
    {
        if (writeValue_ != nullptr and writeValue_->is_object())
        {
            auto& obj = writeValue_->as_object();
            auto it = obj.find(childKey);
            if (it == obj.end())
                return absentMutable(childKey);
            return {&it->value(), childKey};
        }
        if (readValue_ == nullptr or not readValue_->is_object())
            return absentConst(childKey);
        auto const& obj = readValue_->as_object();
        auto it = obj.find(childKey);
        if (it == obj.end())
            return absentConst(childKey);
        return {&it->value(), childKey};
    }

    /**
     * @brief Return a view for an element within this field (must be an array).
     *
     * If this field is absent, not an array, or `idx` is out of bounds, returns
     * an absent view. The returned view inherits the parent key for error context.
     *
     * @param idx Zero-based index of the array element to look up.
     * @return A BoostJsonFieldView for the element, possibly absent.
     */
    [[nodiscard]] BoostJsonFieldView
    element(std::size_t idx) const noexcept
    {
        if (writeValue_ != nullptr and writeValue_->is_array())
        {
            auto& arr = writeValue_->as_array();
            if (idx >= arr.size())
                return absentMutable(key_);
            return {&arr[idx], key_};
        }
        if (readValue_ == nullptr or not readValue_->is_array())
            return absentConst(key_);
        auto const& arr = readValue_->as_array();
        if (idx >= arr.size())
            return absentConst(key_);
        return {&arr[idx], key_};
    }

    /**
     * @brief Type-dispatch predicate: returns true if the field holds a value of type @p T.
     *
     * Supported type arguments: `int64_t`, `uint32_t`, `bool`, `std::string`,
     * `double`, `JsonObject`, `JsonArray`. Any other instantiation is a hard
     * compile-time error.
     *
     * @tparam T The JSON value type to test.
     * @return true if the field is present and holds a value of type @p T.
     */
    template <typename T>
    [[nodiscard]] bool
    is() const noexcept
    {
        if constexpr (std::is_same_v<T, int64_t>)
        {
            return isInt64();
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            return isUint32();
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            return isBool();
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            return isString();
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return isDouble();
        }
        else if constexpr (std::is_same_v<T, JsonObject>)
        {
            return isObject();
        }
        else if constexpr (std::is_same_v<T, JsonArray>)
        {
            return isArray();
        }
        else
        {
            static_assert(false, "unsupported type for is<T>()");
        }
    }

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(int64_t value)
    {
        *writeValue_ = value;
    }

    /**
     * @brief Overwrite the field value with an unsigned 32-bit integer (stored as uint64 in
     * boost::json). @param value The new value.
     */
    void
    set(uint32_t value)
    {
        *writeValue_ = static_cast<uint64_t>(value);  // boost::json stores unsigned as uint64
    }

    /**
     * @brief Overwrite the field value with a string. @param value The new value.
     */
    void
    set(std::string_view value)
    {
        *writeValue_ = boost::json::string{value};
    }

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(bool value)
    {
        *writeValue_ = value;
    }

    /**
     * @brief Overwrite the field value.
     *
     * @param value The new value.
     */
    void
    set(double value)
    {
        *writeValue_ = value;
    }
};

static_assert(SomeFieldView<BoostJsonFieldView>);

/**
 * @brief Non-owning access object for the document root.
 *
 * Distinct from BoostJsonFieldView: the root has no name, is always present,
 * and is only used by RpcSpec/FieldSpec to navigate into named fields via
 * child(). Keeping the type separate prevents passing a keyless View into
 * validators.
 */
class BoostJsonObjectView
{
    boost::json::value const* readValue_;
    boost::json::value* writeValue_;

public:
    /**
     * @brief Construct a mutable object view from a JSON value.
     * @param value The mutable JSON value representing the document root.
     */
    explicit BoostJsonObjectView(boost::json::value& value) noexcept
        : readValue_{&value}, writeValue_{&value}
    {
    }

    /**
     * @brief Construct a read-only object view from a const JSON value.
     * @param value The const JSON value representing the document root.
     */
    explicit BoostJsonObjectView(boost::json::value const& value) noexcept
        : readValue_{&value}, writeValue_{nullptr}
    {
    }

    /**
     * @brief Whether the value is a JSON object.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isObject() const noexcept
    {
        return readValue_->is_object();
    }

    /**
     * @brief Whether the value is a JSON array.
     *
     * @return true when it is; false otherwise.
     */
    [[nodiscard]] bool
    isArray() const noexcept
    {
        return readValue_->is_array();
    }

    /**
     * @brief Return a mutable field view for a named key in the root object.
     *
     * Returns an absent view if the root is not an object or the key is not found.
     *
     * @param key The field name to look up.
     * @return A mutable BoostJsonFieldView for the named field, possibly absent.
     */
    [[nodiscard]] BoostJsonFieldView
    child(std::string_view key) noexcept
    {
        if (writeValue_ != nullptr and writeValue_->is_object())
        {
            auto& obj = writeValue_->as_object();
            if (auto it = obj.find(key); it != obj.end())
                return BoostJsonFieldView{&it->value(), key};
        }
        return BoostJsonFieldView::absentMutable(key);
    }

    /**
     * @brief Return a read-only field view for a named key in the root object.
     *
     * Returns an absent view if the root is not an object or the key is not found.
     *
     * @param key The field name to look up.
     * @return A read-only BoostJsonFieldView for the named field, possibly absent.
     */
    [[nodiscard]] BoostJsonFieldView
    child(std::string_view key) const noexcept
    {
        if (readValue_->is_object())
        {
            auto const& obj = readValue_->as_object();
            if (auto it = obj.find(key); it != obj.end())
                return BoostJsonFieldView{&it->value(), key};
        }
        return BoostJsonFieldView::absentConst(key);
    }
};

static_assert(SomeObjectView<BoostJsonObjectView>);

// Backend-selection aliases. Change these to swap JSON libraries — the spec
// system (RpcSpec, FieldSpec, Validators, Section) is templated on the concepts
// above and is otherwise independent of any concrete JSON type.

/**
 * @brief Active FieldView backend. Swap the alias here to change the JSON library.
 */
using FieldView = BoostJsonFieldView;

/**
 * @brief Active ObjectView backend. Swap the alias here to change the JSON library.
 */
using ObjectView = BoostJsonObjectView;

}  // namespace rpc::spec
