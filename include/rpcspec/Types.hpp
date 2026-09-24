/** @file */
#pragma once

#include <rpcspec/Errors.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace rpc::spec {

/**
 * @brief Result type returned by validators and modifiers.
 */
using MaybeError = std::expected<void, rpc::Status>;

/**
 * @brief A single non-blocking warning emitted by a checker (e.g. field deprecation).
 */
struct Warning
{
    /**
     * @brief Grouping key for the wire-format converter.
     */
    rpc::WarningCode code;  // grouping key for the wire-format converter

    /**
     * @brief Identifier of the field that triggered the warning.
     */
    std::string field;  // identifier for the field that triggered the warning

    /**
     * @brief Extra context appended to the standard message for @ref code.
     */
    std::string message;  // extra context appended to the standard message for `code`
};

/**
 * @brief A collection of non-blocking warnings returned alongside a successful result.
 */
using Warnings = std::vector<Warning>;

// Marker types for use with Type<T> and is<T>() — keeps validators decoupled from any JSON type.
/**
 * @brief Tag type representing a JSON object value; used with Type<T> and is<T>().
 */
struct JsonObject
{
};

/**
 * @brief Tag type representing a JSON array value; used with Type<T> and is<T>().
 */
struct JsonArray
{
};

/**
 * @brief Human-readable name for a JSON/scalar type tag, used by the spec dumper.
 *
 * Primary template intentionally undefined so an unsupported instantiation fails to link/compile
 * — that's a signal to add a specialization here rather than silently emitting a generic name.
 *
 * @tparam T The JSON/scalar type tag to name.
 * @return The name Doxygen-style schema output uses for @p T (e.g. "uint32", "array").
 */
template <typename T>
constexpr std::string_view
typeNameOf() noexcept;

// clang-format off
/**
 * @brief Schema-dump name for this type.
 *
 * @return "int64".
 */
template <> constexpr std::string_view typeNameOf<int64_t>()     noexcept { return "int64"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "int32".
 */
template <> constexpr std::string_view typeNameOf<int32_t>()     noexcept { return "int32"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "uint32".
 */
template <> constexpr std::string_view typeNameOf<uint32_t>()    noexcept { return "uint32"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "uint64".
 */
template <> constexpr std::string_view typeNameOf<uint64_t>()    noexcept { return "uint64"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "bool".
 */
template <> constexpr std::string_view typeNameOf<bool>()        noexcept { return "bool"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "double".
 */
template <> constexpr std::string_view typeNameOf<double>()      noexcept { return "double"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "string".
 */
template <> constexpr std::string_view typeNameOf<std::string>() noexcept { return "string"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "object".
 */
template <> constexpr std::string_view typeNameOf<JsonObject>()  noexcept { return "object"; }

/**
 * @brief Schema-dump name for this type.
 *
 * @return "array".
 */
template <> constexpr std::string_view typeNameOf<JsonArray>()   noexcept { return "array"; }
// clang-format on

}  // namespace rpc::spec
