/** @file */
#pragma once

#include <rpcspec/Errors.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/detail/XrplParse.hpp>

#include <format>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/UintTypes.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace rpc::spec {

struct Required {
    static constexpr std::string_view kName = "required";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Required field '" + std::string{f.key()} + "' missing"
            }};
        }
        return {};
    }
};

template <typename... Ts>
struct Type;

template <>
struct Type<int64_t> {
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
struct Type<bool> {
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
struct Type<std::string> {
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
struct Type<double> {
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
struct Type<uint32_t> {
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
struct Type<JsonObject> {
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
struct Type<JsonArray> {
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
struct Type<T1, T2, Rest...> {
    static constexpr std::string_view kName = "type";

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.paramList(
            "oneOf",
            std::initializer_list<std::string_view>{
                typeNameOf<T1>(), typeNameOf<T2>(), typeNameOf<Rest>()...
            }
        );
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

template <typename T>
    requires(std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, double>)
struct Min {
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
        if constexpr (std::is_same_v<T, int64_t>) {
            if (!f.isInt64())
                return {};
            if (f.asInt64() < bound) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if (!f.isUint32())
                return {};
            if (f.asUint32() < bound) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        } else if constexpr (std::is_same_v<T, double>) {
            if (!f.isDouble())
                return {};
            if (f.asDouble() < bound) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        return {};
    }
};

template <typename T>
Min(T) -> Min<T>;

template <typename T>
    requires(std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, double>)
struct Clamp {
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
        if constexpr (std::is_same_v<T, int64_t>) {
            if (!f.isInt64())
                return {};
            f.set(std::clamp(f.asInt64(), lo, hi));
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if (!f.isUint32())
                return {};
            f.set(static_cast<uint32_t>(std::clamp(f.asUint32(), lo, hi)));
        } else if constexpr (std::is_same_v<T, double>) {
            if (!f.isDouble())
                return {};
            f.set(std::clamp(f.asDouble(), lo, hi));
        }
        return {};
    }
};

template <typename T>
Clamp(T, T) -> Clamp<T>;

template <typename Target>
    requires std::integral<Target> && (!std::is_same_v<Target, bool>)
struct ClampAs {
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

        if (f.isInt64()) {
            auto v = std::clamp(f.asInt64(), kLO, kHI);
            if constexpr (std::is_unsigned_v<Target>) {
                if (v < 0)
                    v = 0;
                f.set(static_cast<uint32_t>(v));
            } else {
                f.set(v);
            }
            return {};
        }

        if (f.isUint32()) {
            if constexpr (std::is_unsigned_v<Target>) {
                auto const u = f.asUint32();
                f.set(static_cast<uint32_t>(std::min<int64_t>(static_cast<int64_t>(u), kHI)));
            } else {
                auto const v = std::min<int64_t>(static_cast<int64_t>(f.asUint32()), kHI);
                f.set(v);
            }
        }
        return {};
    }
};

struct Deprecated {
    static constexpr std::string_view kName = "deprecated";

    template <SomeFieldView FA>
    [[nodiscard]] static std::optional<Warning>
    check(FA const& f)
    {
        if (f.present()) {
            return Warning{
                .code = rpc::WarningCode::WarnRpcDeprecated,
                .field = std::string{f.key()},
                .message = std::format("Field '{}' is deprecated.", f.key())
            };
        }
        return std::nullopt;
    }
};

struct AccountFormat {
    static constexpr std::string_view kName = "account";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"
            }};
        }
        if (!detail::accountFromStringStrict(std::string{f.asString()})) {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcActMalformed, std::string{f.key()} + "Malformed"}
            };
        }
        return {};
    }
};

class TimeFormatValidator final {
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

[[nodiscard]] inline bool
checkIsU32Numeric(std::string_view sv)
{
    uint32_t unused = 0;
    auto [_, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), unused);
    return ec == std::errc();
}

template <typename HexType>
    requires(
        std::is_same_v<HexType, xrpl::uint160> || std::is_same_v<HexType, xrpl::uint192> ||
        std::is_same_v<HexType, xrpl::uint256>
    )
struct HexStringValidator {
    static constexpr std::string_view kName = []() {
        if constexpr (std::is_same_v<HexType, xrpl::uint256>) {
            return std::string_view{"uint256Hex"};
        } else if constexpr (std::is_same_v<HexType, xrpl::uint192>) {
            return std::string_view{"uint192Hex"};
        } else {
            return std::string_view{"uint160Hex"};
        }
    }();

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."
            }};
        }
        HexType parsed;
        if (!parsed.parseHex(std::string{f.asString()}.c_str())) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."
            }};
        }
        return {};
    }
};

using Uint256HexStringValidator = HexStringValidator<xrpl::uint256>;
using Uint192HexStringValidator = HexStringValidator<xrpl::uint192>;
using Uint160HexStringValidator = HexStringValidator<xrpl::uint160>;

struct LedgerIndexValidator {
    static constexpr std::string_view kName = "ledgerIndex";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (f.isInt64() || f.isUint32())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        auto const sv = f.asString();
        if (sv == "validated" || sv == "closed" || sv == "current" || checkIsU32Numeric(sv))
            return {};
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field 'ledger_index', not string or number."
        }};
    }
};

struct AccountBase58Validator {
    static constexpr std::string_view kName = "accountBase58";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"
            }};
        }
        auto const account = detail::parseBase58Wrapper<xrpl::AccountID>(std::string{f.asString()});
        if (!account || account->isZero()) {
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedAddress}};
        }
        return {};
    }
};

struct CurrencyValidator {
    static constexpr std::string_view kName = "currency";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"
            }};
        }
        auto const str = std::string{f.asString()};
        if (str.empty()) {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "IsEmpty"}
            };
        }
        xrpl::Currency currency;
        if (!xrpl::toCurrency(currency, str)) {
            return std::unexpected{
                rpc::Status{rpc::ClioError::RpcMalformedCurrency, "malformedCurrency"}
            };
        }
        return {};
    }
};

struct IssuerValidator {
    static constexpr std::string_view kName = "issuer";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"
            }};
        }
        xrpl::AccountID issuer;
        if (!xrpl::toIssuer(issuer, std::string{f.asString()})) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', bad issuer.", f.key())
            }};
        }
        if (issuer == xrpl::noAccount()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', bad issuer account one.", f.key())
            }};
        }
        return {};
    }
};

struct CurrencyIssueValidator {
    static constexpr std::string_view kName = "currencyIssue";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isObject()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotObject"
            }};
        }
        auto const currFa = f.child("currency");
        if (!currFa.present() || !currFa.isString()) {
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
        }
        xrpl::Currency currency{};
        if (!xrpl::toCurrency(currency, std::string{currFa.asString()})) {
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
        }
        auto const issuerFa = f.child("issuer");
        if (xrpl::isXRP(currency)) {
            if (issuerFa.present()) {
                return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
            }
        } else {
            if (!issuerFa.present() || !issuerFa.isString()) {
                return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
            }
            xrpl::AccountID issuer;
            if (!xrpl::toIssuer(issuer, std::string{issuerFa.asString()})) {
                return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
            }
        }
        return {};
    }
};

struct ToNumberModifier {
    static constexpr std::string_view kName = "toNumber";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    modify(FA& f)
    {
        if (!f.present() || !f.isString())
            return {};
        auto const sv = f.asString();
        if (sv.find('.') != std::string_view::npos) {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        int64_t val = 0;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
        if (ec != std::errc() || ptr != sv.data() + sv.size()) {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        f.set(val);
        return {};
    }
};

struct CredentialTypeValidator {
    static constexpr std::string_view kName = "credentialType";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " NotString"
            }};
        }
        auto const decoded = xrpl::strViewUnHex(f.asString());
        if (!decoded) {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " NotHexString"
            }};
        }
        if (decoded->empty()) {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " is empty"
            }};
        }
        if (decoded->size() > xrpl::kMaxCredentialTypeLength) {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::string{f.key()} + " greater than max length"
            }};
        }
        return {};
    }
};

struct AuthorizeCredentialValidator {
    static constexpr std::string_view kName = "authorizeCredential";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isArray()) {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedRequest, std::string{f.key()} + " not array"
            }};
        }
        auto const sz = f.arraySize();
        if (sz == 0) {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                "Requires at least one element in authorized_credentials array."
            }};
        }
        if (sz > xrpl::kMaxCredentialsArraySize) {
            return std::unexpected{rpc::Status{
                rpc::ClioError::RpcMalformedAuthorizedCredentials,
                std::format(
                    "Max {} number of credentials in authorized_credentials array",
                    xrpl::kMaxCredentialsArraySize
                )
            }};
        }
        for (std::size_t i = 0; i < sz; ++i) {
            auto const elem = f.element(i);
            if (!elem.isObject()) {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials,
                    "authorized_credentials elements in array are not objects."
                }};
            }
            auto const issuerFa = elem.child("issuer");
            if (!issuerFa.present()) {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials,
                    "Field 'Issuer' is required but missing."
                }};
            }
            if (auto err = IssuerValidator::verify(issuerFa); !err) {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials, "issuer NotString"
                }};
            }
            auto const credFa = elem.child("credential_type");
            if (!credFa.present()) {
                return std::unexpected{rpc::Status{
                    rpc::ClioError::RpcMalformedAuthorizedCredentials,
                    "Field 'CredentialType' is required but missing."
                }};
            }
            if (auto err = CredentialTypeValidator::verify(credFa); !err) {
                return err;
            }
        }
        return {};
    }
};

template <typename Fn>
struct CustomValidator {
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

template <typename Fn>
struct CustomModifier {
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

struct NotSupported {
    static constexpr std::string_view kName = "notSupported";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (f.present()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcNotSupported,
                "Not supported field '" + std::string{f.key()} + "'"
            }};
        }
        return {};
    }
};

template <typename T>
    requires(std::is_same_v<T, bool>)
struct NotSupportedIfEqual {
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
        if constexpr (std::is_same_v<T, bool>) {
            if (!f.isBool())
                return {};
            if (f.asBool() != value)
                return {};
        }
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcNotSupported,
            std::format("Not supported field '{}'s value '{}'", f.key(), value)
        }};
    }
};

template <typename T>
NotSupportedIfEqual(T) -> NotSupportedIfEqual<T>;

template <std::size_t N>
struct OneOfValidator {
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
        if (!f.isString()) {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        auto const sv = f.asString();
        for (auto const& v : values) {
            if (sv == v)
                return {};
        }
        return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
    }
};

struct ToLowerModifier {
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

template <typename T>
    requires(std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, double>)
struct Between {
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
        if constexpr (std::is_same_v<T, int64_t>) {
            if (!f.isInt64())
                return {};
            if (f.asInt64() < lo || f.asInt64() > hi) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            if (!f.isUint32())
                return {};
            if (f.asUint32() < lo || f.asUint32() > hi) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        } else if constexpr (std::is_same_v<T, double>) {
            if (!f.isDouble())
                return {};
            if (f.asDouble() < lo || f.asDouble() > hi) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        return {};
    }
};

template <typename T>
Between(T, T) -> Between<T>;

struct Hex256ArrayValidator {
    static constexpr std::string_view kName = "hex256Array";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isArray()) {
            // Mirrors old behaviour: a non-array credentials field is rejected by the leading
            // Type<array> check which produces a plain RpcInvalidParams ("Invalid parameters.").
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        }
        for (std::size_t i = 0; i < f.arraySize(); ++i) {
            auto const elem = f.element(i);
            if (!elem.isString()) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Item is not a valid uint256 type."
                }};
            }
            xrpl::uint256 parsed;
            if (!parsed.parseHex(std::string{elem.asString()}.c_str())) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Item is not a valid uint256 type."
                }};
            }
        }
        return {};
    }
};

struct AccountMarkerValidator {
    static constexpr std::string_view kName = "accountMarker";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"
            }};
        }
        auto const sv = f.asString();
        auto const commaPos = sv.find(',');
        auto const malformed = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."
            }};
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

struct AccountTypeValidator {
    static constexpr std::string_view kName = "accountType";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', not string.", f.key())
            }};
        }
        auto const type =
            detail::accountOwnedLedgerTypeFromStr(std::string{f.asString()});
        if (type == xrpl::ltANY) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::format("Invalid field '{}'.", f.key())
            }};
        }
        return {};
    }
};

struct LedgerEntryTypeValidator {
    static constexpr std::string_view kName = "ledgerType";

    template <SomeFieldView FA>
    [[nodiscard]] static MaybeError
    verify(FA const& f)
    {
        if (!f.present())
            return {};
        if (!f.isString()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                std::format("Invalid field '{}', not string.", f.key())
            }};
        }
        auto const type = detail::ledgerEntryTypeFromStr(std::string{f.asString()});
        if (type == xrpl::ltANY) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::format("Invalid field '{}'.", f.key())
            }};
        }
        return {};
    }
};

}  // namespace rpc::spec
