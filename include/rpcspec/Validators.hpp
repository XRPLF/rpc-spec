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
    static constexpr std::string_view kName = "required";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Required field '" + std::string{f.key()} + "' missing"}};
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

template <>
struct Type<int64_t>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("of", typeNameOf<int64_t>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isInt64())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

template <>
struct Type<bool>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("of", typeNameOf<bool>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isBool())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

template <>
struct Type<std::string>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("of", typeNameOf<std::string>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

template <>
struct Type<double>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("of", typeNameOf<double>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isDouble())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

template <>
struct Type<uint32_t>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("of", typeNameOf<uint32_t>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isUint32())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

template <>
struct Type<JsonObject>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("of", typeNameOf<JsonObject>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isObject())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

template <>
struct Type<JsonArray>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("of", typeNameOf<JsonArray>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isArray())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return {};
    }
};

// OR-semantics: accepts any of the listed types. Returns RpcInvalidParams if none match.
template <typename T1, typename T2, typename... Rest>
struct Type<T1, T2, Rest...>
{
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.paramList(
            "oneOf",
            std::initializer_list<std::string_view>{
                typeNameOf<T1>(), typeNameOf<T2>(), typeNameOf<Rest>()...});
    }

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (f.template is<T1>() || f.template is<T2>() || (f.template is<Rest>() || ...))
            return {};
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
};

/**
 * @brief Validates that a numeric field's value is at least `bound` (inclusive).
 *
 * Silently passes if the field is absent or has a mismatched type (pair with `Type<T>`).
 * Returns `RpcInvalidParams` when the value is strictly less than the bound.
 *
 * @tparam T Numeric type; one of `int64_t`, `uint32_t`, or `double`.
 */
template <typename T>
    requires(std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, double>)
struct Min
{
    static constexpr std::string_view kName = "min";

    T bound;
    consteval explicit Min(T v) : bound{v}
    {
    }

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("bound", bound);
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify(FA const& f) const
    {
        if (!f.present())
            return {};
        if constexpr (std::is_same_v<T, int64_t>)
        {
            if (!f.isInt64())
                return {};
            if (f.asInt64() < bound)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            if (!f.isUint32())
                return {};
            if (f.asUint32() < bound)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            if (!f.isDouble())
                return {};
            if (f.asDouble() < bound)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        return {};
    }
};

template <typename T>
Min(T) -> Min<T>;

/**
 * @brief Modifier that clamps a numeric field's value to the closed interval `[lo, hi]`.
 *
 * Mutates the field in-place. Silently skips absent fields or type mismatches.
 *
 * @tparam T Numeric type; one of `int64_t`, `uint32_t`, or `double`.
 */
template <typename T>
    requires(std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, double>)
struct Clamp
{
    static constexpr std::string_view kName = "clamp";

    T lo, hi;
    consteval Clamp(T l, T h) : lo{l}, hi{h}
    {
    }

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("lo", lo);
        w.param("hi", hi);
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    modify(FA& f) const
    {
        if (!f.present())
            return {};
        if constexpr (std::is_same_v<T, int64_t>)
        {
            if (!f.isInt64())
                return {};
            f.set(std::clamp(f.asInt64(), lo, hi));
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            if (!f.isUint32())
                return {};
            f.set(static_cast<uint32_t>(std::clamp(f.asUint32(), lo, hi)));
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            if (!f.isDouble())
                return {};
            f.set(std::clamp(f.asDouble(), lo, hi));
        }
        return {};
    }
};

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
    requires std::integral<Target> && (!std::is_same_v<Target, bool>)
struct ClampAs
{
    static constexpr std::string_view kName = "clampAs";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("target", typeNameOf<Target>());
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    modify(FA& f) const
    {
        if (!f.present())
            return {};

        constexpr auto kHI = static_cast<int64_t>(std::numeric_limits<Target>::max());
        constexpr auto kLO = static_cast<int64_t>(std::numeric_limits<Target>::min());

        if (f.isInt64())
        {
            auto v = std::clamp(f.asInt64(), kLO, kHI);
            if constexpr (std::is_unsigned_v<Target>)
            {
                if (v < 0)
                    v = 0;
                f.set(static_cast<uint32_t>(v));
            }
            else
            {
                f.set(v);
            }
            return {};
        }

        if (f.isUint32())
        {
            if constexpr (std::is_unsigned_v<Target>)
            {
                auto const u = f.asUint32();
                f.set(static_cast<uint32_t>(std::min<int64_t>(static_cast<int64_t>(u), kHI)));
            }
            else
            {
                auto const v = std::min<int64_t>(static_cast<int64_t>(f.asUint32()), kHI);
                f.set(v);
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
    static constexpr std::string_view kName = "deprecated";

    template <SomeFieldView FA>
    [[nodiscard]] static std::optional<Warning>
    check(FA const& f)
    {
        if (f.present())
        {
            return Warning{
                .code = rpc::WarningCode::WarnRpcDeprecated,
                .field = std::string{f.key()},
                .message = std::format("Field '{}' is deprecated.", f.key())};
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
    static constexpr std::string_view kName = "account";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"}};
        }
        if (!detail::accountFromStringStrict(std::string{f.asString()}))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcActMalformed, std::string{f.key()} + "Malformed"}};
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
    static constexpr std::string_view kName = "timeFormat";

    consteval explicit TimeFormatValidator(std::string_view format) noexcept : format_{format}
    {
    }

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("format", format_);
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify(FA const& f) const
    {
        if (!f.present())
            return {};
        if (!f.isString())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        if (!detail::systemTpFromUtcStr(std::string{f.asString()}, std::string{format_}))
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
    auto [_, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), unused);
    return ec == std::errc();
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
        std::is_same_v<HexType, xrpl::uint160> || std::is_same_v<HexType, xrpl::uint192> ||
        std::is_same_v<HexType, xrpl::uint256>)
struct HexStringValidator
{
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

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        }
        HexType parsed;
        if (!parsed.parseHex(std::string{f.asString()}.c_str()))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
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
    static constexpr std::string_view kName = "ledgerIndex";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (f.isInt64() || f.isUint32())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        auto const sv = f.asString();
        if (sv == "validated" || sv == "closed" || sv == "current" || checkIsU32Numeric(sv))
            return {};
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field 'ledger_index', not string or number."}};
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
    static constexpr std::string_view kName = "accountBase58";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"}};
        }
        auto const account = detail::parseBase58Wrapper<xrpl::AccountID>(std::string{f.asString()});
        if (!account || account->isZero())
        {
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedAddress}};
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
    static constexpr std::string_view kName = "currency";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"}};
        }
        auto const str = std::string{f.asString()};
        if (str.empty())
        {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "IsEmpty"}};
        }
        xrpl::Currency currency;
        if (!xrpl::toCurrency(currency, str))
        {
            return std::unexpected{
                rpc::Status{rpc::ClioError::RpcMalformedCurrency, "malformedCurrency"}};
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
    static constexpr std::string_view kName = "issuer";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"}};
        }
        xrpl::AccountID issuer;
        if (!xrpl::toIssuer(issuer, std::string{f.asString()}))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', bad issuer.", f.key())}};
        }
        if (issuer == xrpl::noAccount())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', bad issuer account one.", f.key())}};
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
    static constexpr std::string_view kName = "currencyIssue";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotObject"}};
        }
        auto const currFa = f.child("currency");
        if (!currFa.present() || !currFa.isString())
        {
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
        }
        xrpl::Currency currency{};
        if (!xrpl::toCurrency(currency, std::string{currFa.asString()}))
        {
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
        }
        auto const issuerFa = f.child("issuer");
        if (xrpl::isXRP(currency))
        {
            if (issuerFa.present())
            {
                return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
            }
        }
        else
        {
            if (!issuerFa.present() || !issuerFa.isString())
            {
                return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
            }
            xrpl::AccountID issuer;
            if (!xrpl::toIssuer(issuer, std::string{issuerFa.asString()}))
            {
                return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
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
    static constexpr std::string_view kName = "toNumber";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    modify(FA& f)
    {
        if (!f.present() || !f.isString())
            return {};
        auto const sv = f.asString();
        if (sv.find('.') != std::string_view::npos)
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        int64_t val = 0;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        if (ec != std::errc() || ptr != sv.data() + sv.size())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        f.set(val);
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
    static constexpr std::string_view kName = "credentialType";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " NotString"}};
        }
        // Materialise a std::string so this compiles against both libxrpl versions:
        // newer libxrpl exposes strUnHex(std::string_view) (accepts a std::string via
        // conversion), while the older one Clio still pins exposes
        // strUnHex(std::string const&) (binds a std::string directly). Passing the
        // string_view from asString() directly would fail against the older overload.
        auto const decoded = xrpl::strUnHex(std::string{f.asString()});
        if (!decoded)
        {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " NotHexString"}};
        }
        if (decoded->empty())
        {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " is empty"}};
        }
        if (decoded->size() > xrpl::kMaxCredentialTypeLength)
        {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " greater than max length"}};
        }
        return {};
    }
};

/**
 * @brief Validates an array of authorized-credential objects, each containing `"issuer"` and
 * `"credential_type"`.
 *
 * Enforces that the field is a non-empty array with at most `xrpl::kMaxCredentialsArraySize`
 * elements, and that each element is an object passing both `IssuerValidator` and
 * `CredentialTypeValidator`. Returns `RpcMalformedRequest` or `RpcMalformedAuthorizedCredentials`
 * on any violation.
 */
struct AuthorizeCredentialValidator
{
    static constexpr std::string_view kName = "authorizeCredential";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isArray())
        {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedRequest, std::string{f.key()} + " not array"}};
        }
        auto const sz = f.arraySize();
        if (sz == 0)
        {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                "Requires at least one element in authorized_credentials array."}};
        }
        if (sz > xrpl::kMaxCredentialsArraySize)
        {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::format(
                    "Max {} number of credentials in authorized_credentials array",
                    xrpl::kMaxCredentialsArraySize)}};
        }
        for (std::size_t i = 0; i < sz; ++i)
        {
            auto const elem = f.element(i);
            if (!elem.isObject())
            {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials,
                    "authorized_credentials elements in array are not objects."}};
            }
            auto const issuerFa = elem.child("issuer");
            if (!issuerFa.present())
            {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials,
                    "Field 'Issuer' is required but missing."}};
            }
            if (auto err = IssuerValidator::verify(issuerFa); !err)
            {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials, "issuer NotString"}};
            }
            auto const credFa = elem.child("credential_type");
            if (!credFa.present())
            {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials,
                    "Field 'CredentialType' is required but missing."}};
            }
            if (auto err = CredentialTypeValidator::verify(credFa); !err)
            {
                return err;
            }
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
 * @tparam Fn A callable type with signature `MaybeError(FA const&)`.
 */
template <typename Fn>
struct CustomValidator
{
    Fn fn;

    consteval explicit CustomValidator(Fn f) : fn{f}
    {
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify(FA const& f) const
    {
        if (!f.present())
            return {};
        return fn(f);
    }
};

template <typename Fn>
CustomValidator(Fn) -> CustomValidator<Fn>;

/**
 * @brief Wraps an arbitrary callable as a field modifier.
 *
 * `fn` is called with a mutable field view when the field is present; absent fields are
 * silently skipped. The callable must return `MaybeError`.
 *
 * @tparam Fn A callable type with signature `MaybeError(FA&)`.
 */
template <typename Fn>
struct CustomModifier
{
    Fn fn;

    consteval explicit CustomModifier(Fn f) : fn{f}
    {
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    modify(FA& f) const
    {
        if (!f.present())
            return {};
        return fn(f);
    }
};

template <typename Fn>
CustomModifier(Fn) -> CustomModifier<Fn>;

/**
 * @brief Rejects any request that includes this field, signalling it is not supported.
 *
 * Returns `RpcNotSupported` with a descriptive message when the field is present.
 */
struct NotSupported
{
    static constexpr std::string_view kName = "notSupported";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (f.present())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcNotSupported,
                "Not supported field '" + std::string{f.key()} + "'"}};
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
    static constexpr std::string_view kName = "notSupportedIf";

    T value;
    consteval explicit NotSupportedIfEqual(T v) : value{v}
    {
    }

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("value", value);
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify(FA const& f) const
    {
        if (!f.present())
            return {};
        if constexpr (std::is_same_v<T, bool>)
        {
            if (!f.isBool())
                return {};
            if (f.asBool() != value)
                return {};
        }
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcNotSupported,
            std::format("Not supported field '{}'s value '{}'", f.key(), value)}};
    }
};

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
    static constexpr std::string_view kName = "oneOf";

    std::array<std::string_view, N> values;

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.paramList("values", values);
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify(FA const& f) const
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        auto const sv = f.asString();
        for (auto const& v : values)
        {
            if (sv == v)
                return {};
        }
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
    static constexpr std::string_view kName = "toLower";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    modify(FA& f)
    {
        if (!f.present() || !f.isString())
            return {};
        auto const sv = f.asString();
        std::string lower{sv};
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        f.set(std::string_view{lower});
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
template <typename T>
    requires(std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, double>)
struct Between
{
    static constexpr std::string_view kName = "between";

    T lo, hi;
    consteval Between(T l, T h) : lo{l}, hi{h}
    {
    }

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("lo", lo);
        w.param("hi", hi);
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify(FA const& f) const
    {
        if (!f.present())
            return {};
        if constexpr (std::is_same_v<T, int64_t>)
        {
            if (!f.isInt64())
                return {};
            if (f.asInt64() < lo || f.asInt64() > hi)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            if (!f.isUint32())
                return {};
            if (f.asUint32() < lo || f.asUint32() > hi)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            if (!f.isDouble())
                return {};
            if (f.asDouble() < lo || f.asDouble() > hi)
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        return {};
    }
};

template <typename T>
Between(T, T) -> Between<T>;

/**
 * @brief Validates that a field is an array of valid `uint256` hex-encoded strings.
 *
 * Returns `RpcInvalidParams` if the field is not an array, or if any element is not a
 * string or fails `xrpl::uint256::parseHex`.
 */
struct Hex256ArrayValidator
{
    static constexpr std::string_view kName = "hex256Array";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isArray())
        {
            // Mirrors old behaviour: a non-array credentials field is rejected by the leading
            // Type<array> check which produces a plain RpcInvalidParams ("Invalid parameters.").
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            auto const elem = f.element(i);
            if (!elem.isString())
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Item is not a valid uint256 type."}};
            }
            xrpl::uint256 parsed;
            if (!parsed.parseHex(std::string{elem.asString()}.c_str()))
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
    static constexpr std::string_view kName = "accountMarker";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"}};
        }
        auto const sv = f.asString();
        auto const commaPos = sv.find(',');
        auto const malformed = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        if (commaPos == std::string_view::npos)
            return malformed();
        auto const hexPart = std::string{sv.substr(0, commaPos)};
        auto const hintPart = sv.substr(commaPos + 1);
        xrpl::uint256 index;
        if (!index.parseHex(hexPart.c_str()))
            return malformed();
        uint64_t hint = 0;
        auto const [ptr, ec] =
            std::from_chars(hintPart.data(), hintPart.data() + hintPart.size(), hint);
        if (ec != std::errc() || ptr != hintPart.data() + hintPart.size())
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
    static constexpr std::string_view kName = "accountType";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', not string.", f.key())}};
        }
        auto const type = accountOwnedLedgerTypeFromStr(std::string{f.asString()});
        if (type == xrpl::ltANY)
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::format("Invalid field '{}'.", f.key())}};
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
    static constexpr std::string_view kName = "default";
    static constexpr bool kIsDefault = true;
    using ValueType = V;

    V value;

    consteval explicit Default(V v) : value{v}
    {
    }

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.param("value", value);
    }
};

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
    static constexpr std::string_view kName = "ledgerType";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', not string.", f.key())}};
        }
        auto const type = ledgerEntryTypeFromStr(std::string{f.asString()});
        if (type == xrpl::ltANY)
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::format("Invalid field '{}'.", f.key())}};
        }
        return {};
    }
};

}  // namespace rpc::spec
