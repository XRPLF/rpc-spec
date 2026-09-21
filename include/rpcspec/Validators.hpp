/** @file */
#pragma once

#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Concepts.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/LedgerTypes.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/detail/XrplParse.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace rpc::spec {

/**
 * @brief Verifies that a field is present in the request.
 *
 * Returns `RpcInvalidParams` with a descriptive message if the field is absent.
 */
struct Required
{
    /**
     * @brief Identifier for this item in the schema dump ("required").
     */
    static constexpr std::string_view kName = "required";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Required field '" + std::string{fieldView.key()} + "' missing"}};
        }
        return {};
    }
};

/**
 * @brief Validates that a present field's JSON type matches the specified C++ type(s).
 *
 * The single-type specialisations (`int64_t`, `bool`, `std::string`, `double`, `uint32_t`,
 * `JsonObject`, `JsonArray`) each accept a field whose JSON representation corresponds to
 * that type. The multi-type specialisation `Type<T1, T2, Rest...>` uses OR semantics: the
 * field is accepted when it matches any one of the listed types. Absent fields are always
 * accepted (use `Required` first if presence is mandatory). Failures return `RpcInvalidParams`.
 *
 * @tparam Ts One or more C++ types to check against. Two or more types enable OR semantics.
 */
template <typename... Ts>
struct Type;

/**
 * @brief The JSON value types a field can be constrained to via `Type<T>` / `is<T>()`.
 *
 * Exactly the set `SomeFieldView::is<T>()` accepts. Naming it keeps an unsupported `Type<T>`
 * a clear constraint failure rather than a static_assert deep inside the field view.
 */
template <typename T>
concept SomeJsonType = std::same_as<T, int64_t> or std::same_as<T, uint32_t> or
    std::same_as<T, bool> or std::same_as<T, double> or std::same_as<T, std::string> or
    std::same_as<T, JsonObject> or std::same_as<T, JsonArray>;

/**
 * @brief Constrains a field to one JSON type, or to any of several (OR semantics).
 */
template <SomeJsonType T>
struct Type<T>
{
    /**
     * @brief Identifier for this item in the schema dump ("type").
     */
    static constexpr std::string_view kName = "type";

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("of", typeNameOf<T>());
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present() or fieldView.template is<T>())
            return {};
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
};

/**
 * @brief Constrains a field to one JSON type, or to any of several (OR semantics).
 */
template <typename T1, typename T2, typename... Rest>
struct Type<T1, T2, Rest...>
{
    /**
     * @brief Identifier for this item in the schema dump ("type").
     */
    static constexpr std::string_view kName = "type";

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.paramList(
            "oneOf",
            std::initializer_list<std::string_view>{
                typeNameOf<T1>(), typeNameOf<T2>(), typeNameOf<Rest>()...});
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (fieldView.template is<T1>() or fieldView.template is<T2>() or
            (fieldView.template is<Rest>() or ...))
            return {};
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
};

/**
 * @brief The numeric types the range validators/modifiers (`Min`, `Between`, `Clamp`) accept.
 */
template <typename T>
concept SomeNumericBound =
    std::same_as<T, int64_t> or std::same_as<T, uint32_t> or std::same_as<T, double>;

namespace detail {

/**
 * @brief Read a numeric field as @p T, or nullopt when absent or of a different JSON type.
 *
 * Collapsing "absent" and "wrong type" into nullopt is what the range validators want: both
 * cases pass silently, leaving the type contract to a paired `Type<T>`.
 *
 * @tparam T The numeric type to read.
 * @param fieldView The field view to read from.
 * @return The value, or nullopt when the field is absent or not a @p T.
 */
template <SomeNumericBound T, SomeFieldView View>
[[nodiscard]] std::optional<T>
numericValue(View const& fieldView)
{
    if (not fieldView.present() or not fieldView.template is<T>())
        return std::nullopt;
    if constexpr (std::is_same_v<T, int64_t>)
    {
        return fieldView.asInt64();
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
        return fieldView.asUint32();
    }
    else
    {
        return fieldView.asDouble();
    }
}

}  // namespace detail

/**
 * @brief Validates that a numeric field's value is at least `bound` (inclusive).
 *
 * Silently passes if the field is absent or has a mismatched type (pair with `Type<T>`).
 * Returns `RpcInvalidParams` when the value is strictly less than the bound.
 *
 * @tparam T Numeric type; one of `int64_t`, `uint32_t`, or `double`.
 */
template <SomeNumericBound T>
struct Min
{
    /**
     * @brief Identifier for this item in the schema dump ("min").
     */
    static constexpr std::string_view kName = "min";

    /**
     * @brief The inclusive bound.
     */
    T bound;

    /**
     * @brief Construct a @ref Min.
     *
     * @param value The inclusive lower bound.
     */
    consteval explicit Min(T value) : bound{value}
    {
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("bound", bound);
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
    {
        auto const value = detail::numericValue<T>(fieldView);
        if (value.has_value() and *value < bound)
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

/**
 * @brief Deduction guide for @ref Min.
 */
template <typename T>
Min(T) -> Min<T>;

/**
 * @brief Modifier that clamps a numeric field's value to the closed interval `[lo, hi]`.
 *
 * Mutates the field in-place. Silently skips absent fields or type mismatches.
 *
 * @tparam T Numeric type; one of `int64_t`, `uint32_t`, or `double`.
 */
template <SomeNumericBound T>
struct Clamp
{
    /**
     * @brief Identifier for this item in the schema dump ("clamp").
     */
    static constexpr std::string_view kName = "clamp";

    /**
     * @brief Inclusive lower bound.
     */
    T lo;

    /**
     * @brief Inclusive upper bound.
     */
    T hi;

    /**
     * @brief Construct a @ref Clamp.
     *
     * @param lo Inclusive lower bound.
     * @param hi Inclusive upper bound.
     */
    consteval Clamp(T lo, T hi) : lo{lo}, hi{hi}
    {
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("lo", lo);
        writer.param("hi", hi);
    }

    /**
     * @brief Normalise the field in place.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to rewrite.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    modify(View& fieldView) const
    {
        if (auto const value = detail::numericValue<T>(fieldView); value.has_value())
            fieldView.set(std::clamp(*value, lo, hi));
        return {};
    }
};

/**
 * @brief Deduction guide for @ref Clamp.
 */
template <typename T>
Clamp(T, T) -> Clamp<T>;

/**
 * @brief Modifier that clamps an `int64_t` or `uint32_t` field to the representable range of
 * `Target`.
 *
 * Useful for safely narrowing a wider integer field to a smaller integral type before
 * downstream processing. Negative values are floored to zero for unsigned `Target` types.
 * Silently skips absent fields and non-integer fields.
 *
 * @tparam Target A non-bool integral type whose min/max define the clamping bounds.
 */
template <typename Target>
    requires std::integral<Target> and (not std::is_same_v<Target, bool>)
struct ClampAs
{
    /**
     * @brief Identifier for this item in the schema dump ("clampAs").
     */
    static constexpr std::string_view kName = "clampAs";

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("target", typeNameOf<Target>());
    }

    /**
     * @brief Normalise the field in place.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to rewrite.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    modify(View& fieldView) const
    {
        if (not fieldView.present())
            return {};

        constexpr auto kHi = static_cast<int64_t>(std::numeric_limits<Target>::max());
        constexpr auto kLo = static_cast<int64_t>(std::numeric_limits<Target>::min());

        if (fieldView.isInt64())
        {
            auto value = std::clamp(fieldView.asInt64(), kLo, kHi);
            if constexpr (std::is_unsigned_v<Target>)
            {
                if (value < 0)
                    value = 0;
                fieldView.set(static_cast<uint32_t>(value));
            }
            else
            {
                fieldView.set(value);
            }
            return {};
        }

        if (fieldView.isUint32())
        {
            if constexpr (std::is_unsigned_v<Target>)
            {
                auto const unsignedValue = fieldView.asUint32();
                fieldView.set(
                    static_cast<uint32_t>(
                        std::min<int64_t>(static_cast<int64_t>(unsignedValue), kHi)));
            }
            else
            {
                auto const value =
                    std::min<int64_t>(static_cast<int64_t>(fieldView.asUint32()), kHi);
                fieldView.set(value);
            }
        }
        return {};
    }
};

/**
 * @brief Emits a `WarnRpcDeprecated` warning when a deprecated field is present.
 *
 * Does not reject the request; the warning is advisory only.
 */
struct Deprecated
{
    /**
     * @brief Identifier for this item in the schema dump ("deprecated").
     */
    static constexpr std::string_view kName = "deprecated";

    /**
     * @brief Inspect the field and optionally raise a non-blocking warning.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to inspect.
     * @return The warning to report, or nullopt when none applies.
     */
    template <SomeFieldView View>
    [[nodiscard]] static std::optional<Warning>
    check(View const& fieldView)
    {
        if (fieldView.present())
        {
            return Warning{
                .code = rpc::WarningCode::WarnRpcDeprecated,
                .field = std::string{fieldView.key()},
                .message = std::format("Field '{}' is deprecated.", fieldView.key())};
        }
        return std::nullopt;
    }
};

/**
 * @brief Validates that a field is a well-formed XRPL account string (base58 or hex).
 *
 * Returns `RpcInvalidParams` with `<key>NotString` if the field is not a string, or
 * `RpcActMalformed` with `<key>Malformed` if the string cannot be parsed as an account.
 */
struct AccountFormat
{
    /**
     * @brief Identifier for this item in the schema dump ("account").
     */
    static constexpr std::string_view kName = "account";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotString"}};
        }
        if (not detail::accountFromStringStrict(std::string{fieldView.asString()}))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcActMalformed, std::string{fieldView.key()} + "Malformed"}};
        }
        return {};
    }
};

/**
 * @brief Validates that a string field matches a specific UTC datetime format.
 *
 * The expected format pattern is provided at construction time (e.g. `"%Y-%m-%dT%H:%M:%S"`).
 * Returns `RpcInvalidParams` if the field is not a string or does not parse against the format.
 */
class TimeFormatValidator final
{
    std::string_view format_;

public:
    /**
     * @brief Identifier for this item in the schema dump ("timeFormat").
     */
    static constexpr std::string_view kName = "timeFormat";

    /**
     * @brief Construct a @ref TimeFormatValidator.
     *
     * @param format The expected strftime-style format.
     */
    consteval explicit TimeFormatValidator(std::string_view format) noexcept : format_{format}
    {
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("format", format_);
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        if (not detail::systemTpFromUtcStr(std::string{fieldView.asString()}, std::string{format_}))
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

/**
 * @brief Returns true if the entire string view represents a valid `uint32_t` decimal number.
 *
 * @param sv The string view to test.
 * @return `true` when `sv` is parseable as a `uint32_t` with no trailing characters.
 */
[[nodiscard]] inline bool
checkIsU32Numeric(std::string_view sv)
{
    uint32_t unused = 0;
    auto const* const begin = sv.data();
    auto const* const end = sv.data() + sv.size();
    auto const [ptr, ec] = std::from_chars(begin, end, unused);
    return ec == std::errc() and ptr == end;
}

/**
 * @brief Validates that a string field is a valid hex encoding of a fixed-width XRPL uint type.
 *
 * Accepted `HexType` values are `xrpl::uint160`, `xrpl::uint192`, and `xrpl::uint256`.
 * Returns `RpcInvalidParams` if the field is not a string or the hex parse fails.
 *
 * @tparam HexType The fixed-width XRPL unsigned integer type to parse against.
 */
template <typename HexType>
    requires(
        std::is_same_v<HexType, xrpl::uint160> or std::is_same_v<HexType, xrpl::uint192> or
        std::is_same_v<HexType, xrpl::uint256>)
struct HexStringValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("kName").
     */
    static constexpr std::string_view kName = []() {
        if constexpr (std::is_same_v<HexType, xrpl::uint256>)
        {
            return std::string_view{"uint256Hex"};
        }
        else if constexpr (std::is_same_v<HexType, xrpl::uint192>)
        {
            return std::string_view{"uint192Hex"};
        }
        else
        {
            return std::string_view{"uint160Hex"};
        }
    }();

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::notStringFieldMessage(fieldView.key())}};
        }
        HexType parsed;
        if (not parsed.parseHex(std::string{fieldView.asString()}.c_str()))
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::malformedFieldMessage(fieldView.key())}};
        }
        return {};
    }
};

/**
 * @brief Convenience alias for `HexStringValidator<xrpl::uint256>`.
 */
using Uint256HexStringValidator = HexStringValidator<xrpl::uint256>;

/**
 * @brief Convenience alias for `HexStringValidator<xrpl::uint192>`.
 */
using Uint192HexStringValidator = HexStringValidator<xrpl::uint192>;

/**
 * @brief Convenience alias for `HexStringValidator<xrpl::uint160>`.
 */
using Uint160HexStringValidator = HexStringValidator<xrpl::uint160>;

/**
 * @brief Validates that a field is an acceptable ledger index specifier.
 *
 * Accepts an integer (`int64_t` or `uint32_t`), the sentinel strings `"validated"`,
 * `"closed"`, `"current"`, or a string containing a decimal `uint32_t`. Returns
 * `RpcInvalidParams` for any other value.
 */
struct LedgerIndexValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("ledgerIndex").
     */
    static constexpr std::string_view kName = "ledgerIndex";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (fieldView.isInt64() or fieldView.isUint32())
            return {};

        auto const unrecognised = [] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, rpc::malformedLedgerIndexMessage()}};
        };

        if (not fieldView.isString())
        {
            // Clio uses one token for every failure mode of this field; xrpld distinguishes
            // them, reporting a wrong JSON type with no message at all.
            if constexpr (kIsClioBuild)
            {
                return unrecognised();
            }
            else
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }

        auto const sv = fieldView.asString();
        if (sv == "validated" or checkIsU32Numeric(sv))
            return {};
        if constexpr (kIsXrpldBuild)
        {
            // Clio rejects these two outright (see ledgerSpecifierFromIndex).
            if (sv == "closed" or sv == "current")
                return {};
        }
        return unrecognised();
    }
};

/**
 * @brief Validates that a string field is a non-zero base58-encoded XRPL `AccountID`.
 *
 * Returns `RpcInvalidParams` if the field is not a string, or `RpcMalformedAddress`
 * if the base58 decode fails or yields the zero account.
 */
struct AccountBase58Validator
{
    /**
     * @brief Identifier for this item in the schema dump ("accountBase58").
     */
    static constexpr std::string_view kName = "accountBase58";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotString"}};
        }
        auto const account =
            detail::parseBase58Wrapper<xrpl::AccountID>(std::string{fieldView.asString()});
        if (not account.has_value() or account->isZero())
        {
            return std::unexpected{rpc::Status{rpc::kMalformedAddress}};
        }
        return {};
    }
};

/**
 * @brief Validates that a string field is a recognised XRPL currency code.
 *
 * Returns `RpcInvalidParams` for non-string or empty values, and `RpcMalformedCurrency`
 * when `xrpl::toCurrency` cannot parse the string.
 */
struct CurrencyValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("currency").
     */
    static constexpr std::string_view kName = "currency";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotString"}};
        }
        auto const str = std::string{fieldView.asString()};
        if (str.empty())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "IsEmpty"}};
        }
        xrpl::Currency currency;
        if (not xrpl::toCurrency(currency, str))
        {
            return std::unexpected{rpc::Status{rpc::kMalformedCurrency, "malformedCurrency"}};
        }
        return {};
    }
};

/**
 * @brief Validates that a string field is a valid, non-`noAccount` XRPL issuer address.
 *
 * Returns `RpcInvalidParams` if the field is not a string, cannot be parsed by
 * `xrpl::toIssuer`, or resolves to the reserved `noAccount()` sentinel.
 */
struct IssuerValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("issuer").
     */
    static constexpr std::string_view kName = "issuer";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotString"}};
        }
        xrpl::AccountID issuer;
        if (not xrpl::toIssuer(issuer, std::string{fieldView.asString()}))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', bad issuer.", fieldView.key())}};
        }
        if (issuer == xrpl::noAccount())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', bad issuer account one.", fieldView.key())}};
        }
        return {};
    }
};

/**
 * @brief Validates a JSON object representing a currency/issuer pair.
 *
 * Expects an object with a `"currency"` string child. For XRP the `"issuer"` child must
 * be absent; for non-XRP currencies a valid `"issuer"` string is required. Returns
 * `RpcMalformedRequest` on any structural or value violation.
 */
struct CurrencyIssueValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("currencyIssue").
     */
    static constexpr std::string_view kName = "currencyIssue";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotObject"}};
        }
        auto const currView = fieldView.child("currency");
        if (not currView.present() or not currView.isString())
        {
            return std::unexpected{rpc::Status{rpc::kMalformedRequest}};
        }
        xrpl::Currency currency{};
        if (not xrpl::toCurrency(currency, std::string{currView.asString()}))
        {
            return std::unexpected{rpc::Status{rpc::kMalformedRequest}};
        }
        auto const issuerView = fieldView.child("issuer");
        if (xrpl::isXRP(currency))
        {
            if (issuerView.present())
            {
                return std::unexpected{rpc::Status{rpc::kMalformedRequest}};
            }
        }
        else
        {
            if (not issuerView.present() or not issuerView.isString())
            {
                return std::unexpected{rpc::Status{rpc::kMalformedRequest}};
            }
            xrpl::AccountID issuer;
            if (not xrpl::toIssuer(issuer, std::string{issuerView.asString()}))
            {
                return std::unexpected{rpc::Status{rpc::kMalformedRequest}};
            }
        }
        return {};
    }
};

/**
 * @brief Modifier that converts a string field containing an integer literal to an `int64_t`.
 *
 * Only operates when the field is present and is a string. Rejects strings that contain a
 * decimal point or cannot be fully parsed as `int64_t`, returning `RpcInvalidParams`.
 */
struct ToNumberModifier
{
    /**
     * @brief Identifier for this item in the schema dump ("toNumber").
     */
    static constexpr std::string_view kName = "toNumber";

    /**
     * @brief Normalise the field in place.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to rewrite.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    modify(View& fieldView)
    {
        if (not fieldView.present() or not fieldView.isString())
            return {};
        auto const sv = fieldView.asString();
        if (sv.find('.') != std::string_view::npos)
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        int64_t val = 0;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        if (ec != std::errc() or ptr != sv.data() + sv.size())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        // Every consumer reads the result back through FieldView::asUint32(), which casts
        // without checking. Reject anything that would silently become a different number.
        if (val < 0 or val > int64_t{std::numeric_limits<uint32_t>::max()})
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        fieldView.set(val);
        return {};
    }
};

/**
 * @brief Validates that a string field is a non-empty hex-encoded credential type within the
 * maximum allowed length.
 *
 * Returns `RpcMalformedAuthorizedCredentials` if the field is not a string, is not valid hex,
 * is empty after decoding, or exceeds `xrpl::kMaxCredentialTypeLength` bytes.
 */
struct CredentialTypeValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("credentialType").
     */
    static constexpr std::string_view kName = "credentialType";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedAuthorizedCredentials, std::string{fieldView.key()} + " NotString"}};
        }
        // Materialise a std::string so this compiles against both libxrpl versions:
        // newer libxrpl exposes strUnHex(std::string_view) (accepts a std::string via
        // conversion), while the older one Clio still pins exposes
        // strUnHex(std::string const&) (binds a std::string directly). Passing the
        // string_view from asString() directly would fail against the older overload.
        auto const decoded = xrpl::strUnHex(std::string{fieldView.asString()});
        if (not decoded.has_value())
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedAuthorizedCredentials,
                std::string{fieldView.key()} + " NotHexString"}};
        }
        if (decoded->empty())
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedAuthorizedCredentials, std::string{fieldView.key()} + " is empty"}};
        }
        if (decoded->size() > xrpl::kMaxCredentialTypeLength)
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedAuthorizedCredentials,
                std::string{fieldView.key()} + " greater than max length"}};
        }
        return {};
    }
};

/**
 * @brief Validates an array of authorized-credential objects, each containing `"issuer"` and
 * `"credential_type"`.
 *
 * Enforces that the field is a non-empty array with at most `xrpl::kMaxCredentialsArraySize`
 * elements, and that each element is an object passing both `AccountBase58Validator` and
 * `CredentialTypeValidator`. Returns `RpcMalformedRequest` or `RpcMalformedAuthorizedCredentials`
 * on any violation.
 */
struct AuthorizeCredentialValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("authorizeCredential").
     */
    static constexpr std::string_view kName = "authorizeCredential";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isArray())
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedRequest, std::string{fieldView.key()} + " not array"}};
        }
        auto const sz = fieldView.arraySize();
        if (sz == 0)
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedAuthorizedCredentials,
                "Requires at least one element in authorized_credentials array."}};
        }
        if (sz > xrpl::kMaxCredentialsArraySize)
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedAuthorizedCredentials,
                std::format(
                    "Max {} number of credentials in authorized_credentials array",
                    xrpl::kMaxCredentialsArraySize)}};
        }
        for (auto i = 0uz; i < sz; ++i)
        {
            auto const elem = fieldView.element(i);
            if (not elem.isObject())
            {
                return std::unexpected{rpc::Status{
                    rpc::kMalformedAuthorizedCredentials,
                    "authorized_credentials elements in array are not objects."}};
            }
            auto const issuerView = elem.child("issuer");
            if (not issuerView.present())
            {
                return std::unexpected{rpc::Status{
                    rpc::kMalformedAuthorizedCredentials,
                    "Field 'Issuer' is required but missing."}};
            }
            if (not AccountBase58Validator::verify(issuerView).has_value())
            {
                return std::unexpected{
                    rpc::Status{rpc::kMalformedAuthorizedCredentials, "issuer NotString"}};
            }
            auto const credView = elem.child("credential_type");
            if (not credView.present())
            {
                return std::unexpected{rpc::Status{
                    rpc::kMalformedAuthorizedCredentials,
                    "Field 'CredentialType' is required but missing."}};
            }
            if (auto res = CredentialTypeValidator::verify(credView); not res.has_value())
                return res;
        }
        return {};
    }
};

/**
 * @brief Wraps an arbitrary callable as a field validator.
 *
 * `fn` is called with the field view when the field is present; absent fields are silently
 * skipped. The callable must return `MaybeError`.
 *
 * @tparam Fn A callable type with signature `MaybeError(View const&)`.
 */
template <typename Fn>
struct CustomValidator
{
    /**
     * @brief The wrapped callable.
     */
    Fn fn;

    /**
     * @brief Construct a @ref CustomValidator.
     *
     * @param fn The callable invoked with a const field view.
     */
    consteval explicit CustomValidator(Fn fn) : fn{fn}
    {
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
    {
        if (not fieldView.present())
            return {};
        return fn(fieldView);
    }
};

/**
 * @brief Deduction guide for @ref CustomValidator.
 */
template <typename Fn>
CustomValidator(Fn) -> CustomValidator<Fn>;

/**
 * @brief Wraps an arbitrary callable as a field modifier.
 *
 * `fn` is called with a mutable field view when the field is present; absent fields are
 * silently skipped. The callable must return `MaybeError`.
 *
 * @tparam Fn A callable type with signature `MaybeError(View&)`.
 */
template <typename Fn>
struct CustomModifier
{
    /**
     * @brief The wrapped callable.
     */
    Fn fn;

    /**
     * @brief Construct a @ref CustomModifier.
     *
     * @param fn The callable invoked with a mutable field view.
     */
    consteval explicit CustomModifier(Fn fn) : fn{fn}
    {
    }

    /**
     * @brief Normalise the field in place.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to rewrite.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    modify(View& fieldView) const
    {
        if (not fieldView.present())
            return {};
        return fn(fieldView);
    }
};

/**
 * @brief Deduction guide for @ref CustomModifier.
 */
template <typename Fn>
CustomModifier(Fn) -> CustomModifier<Fn>;

/**
 * @brief Rejects any request that includes this field, signalling it is not supported.
 *
 * Returns `RpcNotSupported` with a descriptive message when the field is present.
 */
struct NotSupported
{
    /**
     * @brief Identifier for this item in the schema dump ("notSupported").
     */
    static constexpr std::string_view kName = "notSupported";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (fieldView.present())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcNotSupported,
                "Not supported field '" + std::string{fieldView.key()} + "'"}};
        }
        return {};
    }
};

/**
 * @brief Rejects the request when a boolean field is present and equals a specific value.
 *
 * Returns `RpcNotSupported` only when the field is present, is a boolean, and its value
 * matches `value`. Absent fields and non-boolean values are silently accepted.
 *
 * @tparam T Must be `bool` (enforced by constraint).
 */
template <typename T>
    requires(std::is_same_v<T, bool>)
struct NotSupportedIfEqual
{
    /**
     * @brief Identifier for this item in the schema dump ("notSupportedIf").
     */
    static constexpr std::string_view kName = "notSupportedIf";

    /**
     * @brief The value this item carries.
     */
    T value;

    /**
     * @brief Construct a @ref NotSupportedIfEqual.
     *
     * @param value The value that makes the field unsupported.
     */
    consteval explicit NotSupportedIfEqual(T value) : value{value}
    {
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("value", value);
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
    {
        if (not fieldView.present() or not fieldView.isBool() or fieldView.asBool() != value)
            return {};
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcNotSupported,
            std::format("Not supported field '{}'s value '{}'", fieldView.key(), value)}};
    }
};

/**
 * @brief Deduction guide for @ref NotSupportedIfEqual.
 */
template <typename T>
NotSupportedIfEqual(T) -> NotSupportedIfEqual<T>;

/**
 * @brief Validates that a string field's value is one of a compile-time set of allowed strings.
 *
 * Returns `RpcInvalidParams` if the field is not a string or does not match any entry in
 * `values`. Absent fields are silently accepted.
 *
 * @tparam N Number of allowed string values in the set.
 */
template <std::size_t N>
struct OneOfValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("oneOf").
     */
    static constexpr std::string_view kName = "oneOf";

    /**
     * @brief The accepted values.
     */
    std::array<std::string_view, N> values;

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.paramList("values", values);
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        if (std::ranges::contains(values, fieldView.asString()))
            return {};
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
};

/**
 * @brief Modifier that converts a string field's value to lowercase in-place.
 *
 * Silently skips absent fields and non-string fields.
 */
struct ToLowerModifier
{
    /**
     * @brief Identifier for this item in the schema dump ("toLower").
     */
    static constexpr std::string_view kName = "toLower";

    /**
     * @brief Normalise the field in place.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to rewrite.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    modify(View& fieldView)
    {
        if (not fieldView.present() or not fieldView.isString())
            return {};
        std::string lower{fieldView.asString()};
        std::ranges::transform(lower, lower.begin(), [](unsigned char chr) {
            return static_cast<char>(std::tolower(chr));
        });
        fieldView.set(std::string_view{lower});
        return {};
    }
};

/**
 * @brief Validates that a numeric field's value falls within the closed interval `[lo, hi]`.
 *
 * Returns `RpcInvalidParams` when the value is outside the range. Absent fields and
 * type mismatches are silently accepted (pair with `Type<T>` as needed).
 *
 * @tparam T Numeric type; one of `int64_t`, `uint32_t`, or `double`.
 */
template <SomeNumericBound T>
struct Between
{
    /**
     * @brief Identifier for this item in the schema dump ("between").
     */
    static constexpr std::string_view kName = "between";

    /**
     * @brief Inclusive lower bound.
     */
    T lo;

    /**
     * @brief Inclusive upper bound.
     */
    T hi;

    /**
     * @brief Construct a @ref Between.
     *
     * @param lo Inclusive lower bound.
     * @param hi Inclusive upper bound.
     */
    consteval Between(T lo, T hi) : lo{lo}, hi{hi}
    {
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("lo", lo);
        writer.param("hi", hi);
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
    {
        auto const value = detail::numericValue<T>(fieldView);
        if (value.has_value() and (*value < lo or *value > hi))
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

/**
 * @brief Deduction guide for @ref Between.
 */
template <typename T>
Between(T, T) -> Between<T>;

/**
 * @brief Validates a non-empty, bounded array of base58-encoded account IDs.
 *
 * Absent fields pass. A present field must be an array holding between one and @p MaxSize
 * elements, each a base58 account string.
 *
 * @tparam MaxSize The largest number of elements accepted.
 */
template <std::size_t MaxSize>
struct AccountIdArrayValidator
{
    static_assert(MaxSize > 0, "rpcspec: AccountIdArrayValidator needs a non-zero bound");

    /**
     * @brief Identifier for this item in the schema dump ("accountIdArray").
     */
    static constexpr std::string_view kName = "accountIdArray";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};

        if (not fieldView.isArray())
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedField, rpc::expectedFieldMessage(fieldView.key(), "array")}};
        }

        auto const size = fieldView.arraySize();
        if (size == 0 or size > MaxSize)
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedField,
                rpc::expectedFieldMessage(
                    fieldView.key(), std::format("an array of 1 to {} account IDs", MaxSize))}};
        }

        for (auto i = 0uz; i < size; ++i)
        {
            auto const elem = fieldView.element(i);
            if (not elem.isString() or
                not detail::accountFromStringStrict(std::string{elem.asString()}).has_value())
            {
                return std::unexpected{rpc::Status{
                    rpc::kMalformedField,
                    rpc::expectedFieldMessage(fieldView.key(), "an array of account IDs")}};
            }
        }

        return {};
    }
};

/**
 * @brief Validates that a field is an array of valid `uint256` hex-encoded strings.
 *
 * Returns `RpcInvalidParams` if the field is not an array, or if any element is not a
 * string or fails `xrpl::uint256::parseHex`.
 */
struct Hex256ArrayValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("hex256Array").
     */
    static constexpr std::string_view kName = "hex256Array";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isArray())
        {
            // Mirrors old behaviour: a non-array credentials field is rejected by the leading
            // Type<array> check which produces a plain RpcInvalidParams ("Invalid parameters.").
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        auto const size = fieldView.arraySize();
        for (auto i = 0uz; i < size; ++i)
        {
            auto const elem = fieldView.element(i);
            if (not elem.isString())
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Item is not a valid uint256 type."}};
            }
            xrpl::uint256 parsed;
            if (not parsed.parseHex(std::string{elem.asString()}.c_str()))
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Item is not a valid uint256 type."}};
            }
        }
        return {};
    }
};

/**
 * @brief Validates an account-pagination marker in the format `"<uint256hex>,<uint64>"`.
 *
 * The string must contain a comma separating a valid `uint256` hex prefix from a decimal
 * `uint64` page-hint suffix. Returns `RpcInvalidParams` for any structural or parse failure.
 */
struct AccountMarkerValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("accountMarker").
     */
    static constexpr std::string_view kName = "accountMarker";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{fieldView.key()} + "NotString"}};
        }
        auto const sv = fieldView.asString();
        auto const commaPos = sv.find(',');
        auto const malformed = [&] {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::malformedCursorMessage(fieldView.key())}};
        };
        if (commaPos == std::string_view::npos)
            return malformed();
        auto const hexPart = std::string{sv.substr(0, commaPos)};
        auto const hintPart = sv.substr(commaPos + 1);
        xrpl::uint256 index;
        if (not index.parseHex(hexPart.c_str()))
            return malformed();
        uint64_t hint = 0;
        auto const [ptr, ec] =
            std::from_chars(hintPart.data(), hintPart.data() + hintPart.size(), hint);
        if (ec != std::errc() or ptr != hintPart.data() + hintPart.size())
            return malformed();
        return {};
    }
};

/**
 * @brief Validates that a string field names a recognised account-owned ledger object type.
 *
 * Returns `RpcInvalidParams` if the field is not a string or the string does not map to a
 * known ledger type (i.e. would resolve to `xrpl::ltANY`).
 */
struct AccountTypeValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("accountType").
     */
    static constexpr std::string_view kName = "accountType";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', not string.", fieldView.key())}};
        }
        auto const type = accountOwnedLedgerTypeFromStr(std::string{fieldView.asString()});
        if (type == xrpl::ltANY)
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}'.", fieldView.key())}};
        }
        return {};
    }
};

/**
 * @brief Carries the value assigned to a bound Input member when the field is absent.
 *
 * A pure marker item: unlike requirements/modifiers/checks it has no `verify`/`modify`/
 * `check`, so it is a no-op while items run and only participates via `SomeDefault`.
 * `BoundField::parseInto` detects it and, when the field is omitted from the request,
 * assigns `value` to the bound member — making the spec (not the Input struct's member
 * initialiser) the single source of truth for a field's default. Build via `defaultTo`.
 *
 * @tparam V The default value type; must be assignable to the bound member.
 */
template <typename V>
struct Default
{
    /**
     * @brief Identifier for this item in the schema dump ("default").
     */
    static constexpr std::string_view kName = "default";

    /**
     * @brief Marks this item as a pure default carrier (see SomeDefault).
     */
    static constexpr bool kIsDefault = true;

    /**
     * @brief The value this converter produces (`V`).
     */
    using ValueType = V;

    /**
     * @brief The value this item carries.
     */
    V value;

    /**
     * @brief Construct a @ref Default.
     *
     * @param value The value assigned when the field is absent.
     */
    consteval explicit Default(V value) : value{value}
    {
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.param("value", value);
    }
};

/**
 * @brief Deduction guide for @ref Default.
 */
template <typename V>
Default(V) -> Default<V>;

/**
 * @brief Validates that a string field names a recognised ledger entry type.
 *
 * Returns `RpcInvalidParams` if the field is not a string or the string does not map to a
 * known ledger entry type (i.e. would resolve to `xrpl::ltANY`).
 */
struct LedgerEntryTypeValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("ledgerType").
     */
    static constexpr std::string_view kName = "ledgerType";

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] static MaybeError
    verify(View const& fieldView)
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', not string.", fieldView.key())}};
        }
        auto const type = ledgerEntryTypeFromStr(std::string{fieldView.asString()});
        if (type == xrpl::ltANY)
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}'.", fieldView.key())}};
        }
        return {};
    }
};

}  // namespace rpc::spec
