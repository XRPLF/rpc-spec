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
 * FieldSpec obtains the FA via the root's child(key); validators never see the
 * raw JSON type.
 *
 * Two constructors carry const-correctness through:
 *   - mutable ctor (from value&):       both readValue_ and writeValue_ are set
 *   - const ctor   (from value const&): only readValue_ is set; set() is
 * unreachable during check() since callIfChecker passes FA const& which
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
     * @param v Pointer to the JSON value (may be null when the field is absent).
     * @param k The field key, used in error messages and as the view's identity.
     */
    BoostJsonFieldView(boost::json::value* v, std::string_view k) noexcept
        : readValue_{v}, writeValue_{v}, key_{k}
    {
    }

    /**
     * @brief Construct a read-only view: write access is disabled.
     *
     * @param v Pointer to the const JSON value (may be null when the field is absent).
     * @param k The field key, used in error messages and as the view's identity.
     */
    BoostJsonFieldView(boost::json::value const* v, std::string_view k) noexcept
        : readValue_{v}, writeValue_{nullptr}, key_{k}
    {
    }

    [[nodiscard]] std::string_view
    key() const noexcept
    {
        return key_;
    }

    [[nodiscard]] bool
    present() const noexcept
    {
        return readValue_ != nullptr;
    }

    [[nodiscard]] bool
    isInt64() const noexcept
    {
        return readValue_ != nullptr &&
            (readValue_->is_int64() ||
             (readValue_->is_uint64() &&
              readValue_->as_uint64() <=
                  static_cast<uint64_t>(std::numeric_limits<int64_t>::max())));
    }

    [[nodiscard]] int64_t
    asInt64() const
    {
        return readValue_->is_int64() ? readValue_->as_int64()
                                      : static_cast<int64_t>(readValue_->as_uint64());
    }

    [[nodiscard]] bool
    isUint32() const noexcept
    {
        if (readValue_ == nullptr)
            return false;
        if (readValue_->is_uint64())
            return readValue_->as_uint64() <= std::numeric_limits<uint32_t>::max();
        if (readValue_->is_int64())
        {
            auto const v = readValue_->as_int64();
            return v >= 0 && v <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
        }
        return false;
    }

    [[nodiscard]] uint32_t
    asUint32() const
    {
        return readValue_->is_uint64() ? static_cast<uint32_t>(readValue_->as_uint64())
                                       : static_cast<uint32_t>(readValue_->as_int64());
    }

    [[nodiscard]] bool
    isBool() const noexcept
    {
        return readValue_ != nullptr && readValue_->is_bool();
    }

    [[nodiscard]] bool
    asBool() const
    {
        return readValue_->as_bool();
    }

    [[nodiscard]] bool
    isString() const noexcept
    {
        return readValue_ != nullptr && readValue_->is_string();
    }

    [[nodiscard]] std::string_view
    asString() const
    {
        return readValue_->as_string();
    }

    [[nodiscard]] bool
    isDouble() const noexcept
    {
        return readValue_ != nullptr && readValue_->is_double();
    }

    [[nodiscard]] double
    asDouble() const
    {
        return readValue_->as_double();
    }

    [[nodiscard]] bool
    isObject() const noexcept
    {
        return readValue_ != nullptr && readValue_->is_object();
    }

    [[nodiscard]] bool
    isArray() const noexcept
    {
        return readValue_ != nullptr && readValue_->is_array();
    }

    [[nodiscard]] std::size_t
    arraySize() const noexcept
    {
        if (readValue_ == nullptr || !readValue_->is_array())
            return 0;
        return readValue_->as_array().size();
    }

    [[nodiscard]] std::size_t
    objectSize() const noexcept
    {
        if (readValue_ == nullptr || !readValue_->is_object())
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
        if (writeValue_ != nullptr && writeValue_->is_object())
        {
            auto& obj = writeValue_->as_object();
            auto it = obj.find(childKey);
            if (it == obj.end())
                return {static_cast<boost::json::value*>(nullptr), childKey};
            return {&it->value(), childKey};
        }
        if (readValue_ == nullptr || !readValue_->is_object())
            return {static_cast<boost::json::value const*>(nullptr), childKey};
        auto const& obj = readValue_->as_object();
        auto it = obj.find(childKey);
        if (it == obj.end())
            return {static_cast<boost::json::value const*>(nullptr), childKey};
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
        if (writeValue_ != nullptr && writeValue_->is_array())
        {
            auto& arr = writeValue_->as_array();
            if (idx >= arr.size())
                return {static_cast<boost::json::value*>(nullptr), key_};
            return {&arr[idx], key_};
        }
        if (readValue_ == nullptr || !readValue_->is_array())
            return {static_cast<boost::json::value const*>(nullptr), key_};
        auto const& arr = readValue_->as_array();
        if (idx >= arr.size())
            return {static_cast<boost::json::value const*>(nullptr), key_};
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

    /** @brief Overwrite the field value with a signed 64-bit integer. @param v The new value. */
    void
    set(int64_t v)
    {
        *writeValue_ = v;
    }

    /** @brief Overwrite the field value with an unsigned 32-bit integer (stored as uint64 in
     * boost::json). @param v The new value. */
    void
    set(uint32_t v)
    {
        *writeValue_ = static_cast<uint64_t>(v);  // boost::json stores unsigned as uint64
    }

    /** @brief Overwrite the field value with a string. @param v The new value. */
    void
    set(std::string_view v)
    {
        *writeValue_ = boost::json::string{v};
    }

    /** @brief Overwrite the field value with a boolean. @param v The new value. */
    void
    set(bool v)
    {
        *writeValue_ = v;
    }

    /** @brief Overwrite the field value with a double. @param v The new value. */
    void
    set(double v)
    {
        *writeValue_ = v;
    }
};

static_assert(SomeFieldView<BoostJsonFieldView>);

/**
 * @brief Non-owning access object for the document root.
 *
 * Distinct from BoostJsonFieldView: the root has no name, is always present,
 * and is only used by RpcSpec/FieldSpec to navigate into named fields via
 * child(). Keeping the type separate prevents passing a keyless FA into
 * validators.
 */
class BoostJsonObjectView
{
    boost::json::value const* readValue_;
    boost::json::value* writeValue_;

public:
    /**
     * @brief Construct a mutable object view from a JSON value.
     * @param v The mutable JSON value representing the document root.
     */
    explicit BoostJsonObjectView(boost::json::value& v) noexcept : readValue_{&v}, writeValue_{&v}
    {
    }

    /**
     * @brief Construct a read-only object view from a const JSON value.
     * @param v The const JSON value representing the document root.
     */
    explicit BoostJsonObjectView(boost::json::value const& v) noexcept
        : readValue_{&v}, writeValue_{nullptr}
    {
    }

    [[nodiscard]] bool
    isObject() const noexcept
    {
        return readValue_->is_object();
    }

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
        if (writeValue_ != nullptr && writeValue_->is_object())
        {
            auto& obj = writeValue_->as_object();
            if (auto it = obj.find(key); it != obj.end())
                return BoostJsonFieldView{&it->value(), key};
        }
        return BoostJsonFieldView{static_cast<boost::json::value*>(nullptr), key};
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
        return BoostJsonFieldView{static_cast<boost::json::value const*>(nullptr), key};
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
