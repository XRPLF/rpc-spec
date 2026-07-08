/** @file */
#pragma once

#include <rpcspec/Concepts.hpp>
#include <rpcspec/IfType.hpp>
#include <rpcspec/Section.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/WithCustomError.hpp>

#include <array>
#include <string>
#include <string_view>

namespace rpc::spec {

// NOLINTBEGIN(readability-identifier-naming)
/** @brief Marks a field as required; fails validation if the field is absent. */
inline constexpr auto required = Required{};
/** @brief Marks a field as deprecated; emits a deprecation warning when the field is present. */
inline constexpr auto deprecated = Deprecated{};
/** @brief Validates that a field contains a valid XRPL account address. */
inline constexpr auto account = AccountFormat{};

/**
 * @brief Constrains a field to one or more acceptable C++ types.
 *
 * @tparam Ts The set of acceptable types (e.g. `type<std::string>`, `type<bool, std::string>`).
 */
template <typename... Ts>
inline constexpr auto type = Type<Ts...>{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief Requires a numeric field value to be at least @p v.
 *
 * @tparam T Numeric type of the bound.
 * @param v  The inclusive lower bound.
 * @return   A `Min` validator configured with @p v.
 */
template <typename T>
consteval auto
min(T v)
{
    return Min{v};
}

/**
 * @brief Clamps a numeric field value to the closed interval [@p lo, @p hi].
 *
 * @tparam T Numeric type of the bounds.
 * @param lo Inclusive lower bound.
 * @param hi Inclusive upper bound.
 * @return   A `Clamp` modifier configured with the given bounds.
 */
template <typename T>
consteval auto
clamp(T lo, T hi)
{
    return Clamp{lo, hi};
}

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Clamps a numeric field value and stores the result as type @p Target.
 *
 * @tparam Target The numeric type to which the clamped value is converted.
 */
template <typename Target>
inline constexpr auto clampAs = ClampAs<Target>{};

/**
 * @brief Applies sub-processors only when the field's runtime JSON type is @p T.
 *
 * @tparam T        The JSON type to branch on (e.g. `std::string`, `JsonObject`).
 * @tparam SubItems Processor types to apply when the type matches.
 * @param  items    The processors to run conditionally.
 * @return          An `IfType` modifier.
 */
template <typename T, SomeProcessor... SubItems>
consteval auto
ifType(SubItems... items)
{
    return IfType<T, SubItems...>{items...};
}

/**
 * @brief Wraps a requirement or modifier and replaces its error with a custom one.
 *
 * @tparam Wrapped  A type satisfying `SomeRequirement` or `SomeModifier`.
 * @param  w        The processor whose error to replace.
 * @param  code     The `rpc::CombinedError` code to report on failure.
 * @param  message  Optional message appended to the status (defaults to empty).
 * @return          A `WithCustomError` wrapper.
 */
template <typename Wrapped>
consteval auto
withCustomError(Wrapped w, rpc::CombinedError code, std::string_view message = {})
{
    return WithCustomError<Wrapped>{w, code, message};
}

/**
 * @brief Validates that a string field matches the given `strftime`-style format.
 *
 * @param format The expected date/time format string.
 * @return       A `TimeFormatValidator` configured with @p format.
 */
consteval auto
timeFormat(std::string_view format)
{
    return TimeFormatValidator{format};
}

/**
 * @brief Validates and processes a nested JSON object field using a set of sub-field specs.
 *
 * @tparam SubFields `FieldSpec` types describing the fields inside the sub-object.
 * @param  sf        The sub-field specs to apply.
 * @return           A `Section` modifier.
 */
template <typename... SubFields>
consteval auto
section(SubFields... sf)
{
    return Section<SubFields...>{sf...};
}

/**
 * @brief Applies the given validators only in Clio server builds.
 *
 * Has no effect (and zero overhead) when compiled for rippled.
 *
 * @tparam Vs Processor types to apply conditionally.
 * @param  vs The processors to run in Clio builds.
 * @return    An `IfServerClioValidator` wrapper.
 */
template <typename... Vs>
consteval auto
ifServerClio(Vs... vs)
{
    return IfServerClioValidator<Vs...>{vs...};
}

/**
 * @brief Applies the given validators only in rippled server builds.
 *
 * Has no effect (and zero overhead) when compiled for Clio.
 *
 * @tparam Vs Processor types to apply conditionally.
 * @param  vs The processors to run in rippled builds.
 * @return    An `IfServerXrpldValidator` wrapper.
 */
template <typename... Vs>
consteval auto
ifServerXrpld(Vs... vs)
{
    return IfServerXrpldValidator<Vs...>{vs...};
}

// NOLINTBEGIN(readability-identifier-naming)
/** @brief Validates that a field contains a valid ledger index (integer or
 * "current"/"closed"/"validated"). */
inline constexpr auto ledgerIndex = LedgerIndexValidator{};
/** @brief Validates that a field contains a base58-encoded XRPL account address. */
inline constexpr auto accountBase58 = AccountBase58Validator{};
/** @brief Validates that a field contains a valid XRPL currency code. */
inline constexpr auto currency = CurrencyValidator{};
/** @brief Validates that a field contains a valid XRPL issuer account address. */
inline constexpr auto issuer = IssuerValidator{};
/** @brief Validates that a field contains a valid XRPL currency+issuer pair. */
inline constexpr auto currencyIssue = CurrencyIssueValidator{};
/** @brief Validates that a field contains a valid XRPL credential type string. */
inline constexpr auto credentialType = CredentialTypeValidator{};
/** @brief Validates that a field contains a valid XRPL authorize-credential object. */
inline constexpr auto authorizeCredential = AuthorizeCredentialValidator{};
/** @brief Modifier that converts a string field value to a number in place. */
inline constexpr auto toNumber = ToNumberModifier{};
/** @brief Validates that a field contains a 256-bit value encoded as a 64-character hex string. */
inline constexpr auto uint256Hex = Uint256HexStringValidator{};
/** @brief Validates that a field contains a 192-bit value encoded as a 48-character hex string. */
inline constexpr auto uint192Hex = Uint192HexStringValidator{};
/** @brief Validates that a field contains a 160-bit value encoded as a 40-character hex string. */
inline constexpr auto uint160Hex = Uint160HexStringValidator{};
/** @brief Marks a field as not supported; always returns an error when the field is present. */
inline constexpr auto notSupported = NotSupported{};

/**
 * @brief Marks a field as not supported when its value equals @p value.
 *
 * @tparam T   Type of the disallowed value.
 * @param  value The specific value that triggers the not-supported error.
 * @return     A `NotSupportedIfEqual` validator.
 */
template <typename T>
consteval auto
notSupportedIf(T value)
{
    return NotSupportedIfEqual{value};
}

/** @brief Modifier that converts a string field value to lowercase in place. */
inline constexpr auto toLower = ToLowerModifier{};
/** @brief Validates that a field contains a JSON array of 256-bit hex strings. */
inline constexpr auto hex256Array = Hex256ArrayValidator{};
/** @brief Validates that a field contains a valid XRPL account object marker. */
inline constexpr auto accountMarker = AccountMarkerValidator{};
/** @brief Validates that a field contains a recognised XRPL account type string. */
inline constexpr auto accountType = AccountTypeValidator{};
/** @brief Validates that a field contains a recognised XRPL ledger entry type string. */
inline constexpr auto ledgerType = LedgerEntryTypeValidator{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief Validates that a field's value is one of a fixed set of string literals.
 *
 * @tparam T       The expected C++ type of the field (default: `std::string`).
 * @tparam Strings Deduced string-literal types.
 * @param  vals    The allowed string values.
 * @return         A `OneOfValidator` configured with @p vals.
 */
template <typename T = std::string, typename... Strings>
consteval auto
oneOf(Strings... vals)
{
    return OneOfValidator<sizeof...(Strings)>{
        std::array<std::string_view, sizeof...(Strings)>{std::string_view{vals}...}};
}

/**
 * @brief Validates that a numeric field value lies within the closed interval [@p lo, @p hi].
 *
 * Unlike `clamp`, this validator rejects out-of-range values instead of clamping them.
 *
 * @tparam T  Numeric type of the bounds.
 * @param  lo Inclusive lower bound.
 * @param  hi Inclusive upper bound.
 * @return    A `Between` validator configured with the given bounds.
 */
template <typename T>
consteval auto
between(T lo, T hi)
{
    return Between{lo, hi};
}

/**
 * @brief Wraps a callable as a field modifier.
 *
 * @tparam Fn Callable type; must accept a mutable field-view reference.
 * @param  f  The callable to invoke during the modify phase.
 * @return    A `CustomModifier` wrapping @p f.
 */
template <typename Fn>
consteval auto
customModifier(Fn f)
{
    return CustomModifier<Fn>{f};
}

/**
 * @brief Supplies the value assigned to a bound Input member when the field is absent.
 *
 * Moves a field's default out of the Input struct's member initialiser and into the spec,
 * so the spec is the single source of truth for the whole field contract (min/max *and*
 * default). Only meaningful on a bound `field()` (one with a pointer-to-member); the value
 * type must be assignable to that member (enforced at compile time in `BoundField`).
 *
 * @code
 * field("limit", &Input::limit, type<uint32_t>, clamp(kMin, kMax), defaultTo(kDefault), asUint32)
 * @endcode
 *
 * @tparam V  The default value type.
 * @param  v  The value assigned when the field is omitted from the request.
 * @return    A `Default` field item.
 */
template <typename V>
consteval auto
defaultTo(V v)
{
    return Default<V>{v};
}

}  // namespace rpc::spec
