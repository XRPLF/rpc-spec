/** @file */
#pragma once
// Typed converters for the parse() path (see Typed.hpp).
//
// A converter both VALIDATES a field and PRODUCES its strong-typed value in a
// single pass — its `parse()` result is what gets assigned into the bound Input
// member. This removes the validate-then-separately-deserialize double work and,
// because the produced type is strong (e.g. xrpl::AccountID rather than a
// re-parseable std::string), it removes the class of handler crashes that came
// from re-deriving/asserting input shape after validation.

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Concepts.hpp>
#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/detail/XrplParse.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace rpc::spec {

/**
 * @brief Result of a converter: the produced strong value, or a Status error.
 */
template <typename T>
using Parsed = std::expected<T, rpc::Status>;

/**
 * @brief Converts an account/ident field into a strong xrpl::AccountID.
 *
 * The validation work (base58/hex decode) IS the conversion — the handler
 * receives a ready AccountID, never a string it must re-parse.
 */
struct AccountIdConverter
{
    static constexpr std::string_view kName = "account";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"}};
        }
        auto id = detail::accountFromStringStrict(std::string{f.asString()});
        if (!id)
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcActMalformed, std::string{f.key()} + "Malformed"}};
        }
        return *id;
    }
};

/**
 * @brief Validates a uint256 hex field and yields it as a std::string.
 *
 * Kept as a string (rather than xrpl::uint256) so existing helper signatures
 * such as getLedgerHeaderFromHashOrSeq are unaffected; the value is guaranteed
 * to be a well-formed hash.
 */
struct LedgerHashConverter
{
    static constexpr std::string_view kName = "uint256Hex";
    using ValueType = std::string;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        if (!f.isString())
            return err();
        xrpl::uint256 parsed;
        if (!parsed.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return std::string{f.asString()};
    }
};

/**
 * @brief Validates a uint256 hex field and yields it as a strong xrpl::uint256.
 *
 * Like LedgerHashConverter, but produces the strong type so the handler receives
 * a ready hash and never re-parses the string.
 */
struct Uint256HexConverter
{
    static constexpr std::string_view kName = "uint256Hex";
    using ValueType = xrpl::uint256;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        if (!f.isString())
            return err();
        xrpl::uint256 parsed;
        if (!parsed.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return parsed;
    }
};

/**
 * @brief Validates a uint192 hex field and yields it as a strong xrpl::uint192.
 *
 * The uint192 form of @ref Uint256HexConverter, used for MPT issuance ids.
 */
struct Uint192HexConverter
{
    static constexpr std::string_view kName = "uint192Hex";
    using ValueType = xrpl::uint192;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        if (!f.isString())
            return err();
        xrpl::uint192 parsed;
        if (!parsed.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return parsed;
    }
};

/**
 * @brief Converts a ledger_index field into an optional<uint32_t>.
 *
 * Mirrors util::getLedgerIndex semantics: the sentinels "validated"/"closed"/
 * "current" and out-of-uint32-range numbers resolve to no concrete index
 * (nullopt — the handler then falls back to the latest sequence); other
 * non-numeric strings are an error.
 */
struct LedgerIndexOptConverter
{
    static constexpr std::string_view kName = "ledgerIndex";
    using ValueType = std::optional<uint32_t>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isUint32())
            return std::optional<uint32_t>{f.asUint32()};
        if (f.isInt64())  // numeric but out of uint32 range → leave unset
            return std::optional<uint32_t>{std::nullopt};
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field 'ledger_index', not string or number."}};
        }
        auto const sv = f.asString();
        if (sv == "validated" || sv == "closed" || sv == "current")
            return std::optional<uint32_t>{std::nullopt};
        uint32_t out = 0;
        auto const* const begin = sv.data();
        auto const* const end = sv.data() + sv.size();
        if (auto const [p, ec] = std::from_chars(begin, end, out); ec == std::errc{} && p == end)
            return std::optional<uint32_t>{out};
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field 'ledger_index', not string or number."}};
    }
};

/**
 * @brief Converts a field into a JsonBool.
 *
 * @tparam Strict When true (V2 APIs) the field must be a JSON bool; when false
 * (V1 APIs) any JSON scalar is leniently coerced, matching JsonBool's historical
 * tag_invoke behaviour.
 */
template <bool Strict>
struct JsonBoolConverterT
{
    static constexpr std::string_view kName = "bool";
    using ValueType = JsonBool;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if constexpr (Strict)
        {
            if (!f.isBool())
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            return JsonBool{f.asBool()};
        }
        else
        {
            if (f.isBool())
                return JsonBool{f.asBool()};
            if (f.isUint32())
                return JsonBool{f.asUint32() != 0};
            if (f.isInt64())
                return JsonBool{f.asInt64() != 0};
            if (f.isDouble())
                return JsonBool{f.asDouble() != 0.0};
            if (f.isString())
            {
                auto const s = f.asString();
                return JsonBool{!s.empty() && s[0] != 0};
            }
            if (f.isArray())
                return JsonBool{f.arraySize() != 0};
            if (f.isObject())
                return JsonBool{f.objectSize() != 0};
            return JsonBool{false};
        }
    }
};

/**
 * @brief Validates a field is a uint32 and yields it.
 *
 * Pairs with the `clamp` modifier for limit-style fields: clamp normalises the
 * incoming number in place, then this converts the clamped value.
 */
struct Uint32Converter
{
    static constexpr std::string_view kName = "uint32";
    using ValueType = uint32_t;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isUint32())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return f.asUint32();
    }
};

/**
 * @brief Validates a field is a string and yields it (e.g. after a `toLower` modifier).
 */
struct StringConverter
{
    static constexpr std::string_view kName = "string";
    using ValueType = std::string;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString())
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        return std::string{f.asString()};
    }
};

/**
 * @brief Like AccountIdConverter, but maps every failure (non-string or malformed)
 * to RpcActMalformed with no message — i.e. the xrpld default "Account malformed.".
 *
 * Mirrors the legacy `withCustomError(account, RpcActMalformed)` field pattern used
 * by handlers (account_lines, account_mptoken_issuances, …) that want the uniform
 * "Account malformed." error rather than the per-key "<key>NotString"/"<key>Malformed".
 */
struct AccountIdActMalformedConverter
{
    static constexpr std::string_view kName = "account";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
        {
            if (auto id = detail::accountFromStringStrict(std::string{f.asString()}); id)
                return *id;
        }
        return std::unexpected{rpc::Status{rpc::RippledError::RpcActMalformed}};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/** @brief Converter instance: validates and decodes an account field into xrpl::AccountID with
 * per-key error messages. */
inline constexpr auto accountId = AccountIdConverter{};
/** @brief Converter instance: validates and decodes an account field into xrpl::AccountID, mapping
 * all failures to RpcActMalformed. */
inline constexpr auto accountIdActMalformed = AccountIdActMalformedConverter{};
/** @brief Converter instance: validates a field is a uint32 and yields it. */
inline constexpr auto asUint32 = Uint32Converter{};
/** @brief Converter instance: validates a field is a string and yields it. */
inline constexpr auto asString = StringConverter{};
/** @brief Converter instance: validates and decodes a hex-encoded uint256 field into a std::string.
 */
inline constexpr auto ledgerHashHex = LedgerHashConverter{};
/** @brief Converter instance: validates a hex-encoded uint256 field and yields a strong
 * xrpl::uint256. */
inline constexpr auto asUint256 = Uint256HexConverter{};
/** @brief Converter instance: validates a hex-encoded uint192 field and yields a strong
 * xrpl::uint192. */
inline constexpr auto asUint192 = Uint192HexConverter{};
/** @brief Converter instance: decodes a ledger_index field into an optional uint32 (nullopt for
 * sentinel strings). */
inline constexpr auto ledgerIndexOpt = LedgerIndexOptConverter{};
/** @brief Converter instance: lenient bool converter (any JSON scalar coerced to bool; V1 API
 * semantics). */
inline constexpr auto jsonBool = JsonBoolConverterT<false>{};
/** @brief Converter instance: strict bool converter (field must be a JSON bool; V2 API semantics).
 */
inline constexpr auto jsonBoolStrict = JsonBoolConverterT<true>{};
// NOLINTEND(readability-identifier-naming)

}  // namespace rpc::spec
