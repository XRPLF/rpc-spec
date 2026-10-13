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
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/detail/XrplParse.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

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
    /**
     * @brief Identifier for this item in the schema dump ("account").
     */
    static constexpr std::string_view kName = "account";

    /**
     * @brief The value this converter produces (`xrpl::AccountID`).
     */
    using ValueType = xrpl::AccountID;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams, rpc::notStringFieldMessage(fieldView.key())}};
        }
        if constexpr (kIsXrpldBuild)
        {
            auto id = xrpl::parseBase58<xrpl::AccountID>(std::string{fieldView.asString()});
            if (not id.has_value())
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcActMalformed}};
            return *id;
        }
        else
        {
            auto id = detail::accountFromStringStrict(std::string{fieldView.asString()});
            if (not id.has_value())
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcActMalformed, std::string{fieldView.key()} + "Malformed"}};
            }
            return *id;
        }
    }
};

/**
 * @brief Shared body of the hex-string converters: parse @p HexType, yield @p Value.
 *
 * All three concrete converters differ only in the width they parse and in whether they hand
 * back the strong type or the original string, so that is all the derived types supply.
 *
 * @tparam HexType The fixed-width XRPL unsigned integer to parse the field as.
 * @tparam Value The produced value type (`HexType`, or `std::string` to keep the raw text).
 */
template <typename HexType, typename Value>
struct HexConverterBase
{
    /**
     * @brief The value this converter produces (`Value`).
     */
    using ValueType = Value;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
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
        if constexpr (std::is_same_v<Value, std::string>)
        {
            return std::string{fieldView.asString()};
        }
        else
        {
            return parsed;
        }
    }
};

/**
 * @brief Validates a uint256 hex field and yields it as a std::string.
 *
 * Kept as a string (rather than xrpl::uint256) so existing helper signatures
 * such as getLedgerHeaderFromHashOrSeq are unaffected; the value is guaranteed
 * to be a well-formed hash.
 */
struct LedgerHashConverter : HexConverterBase<xrpl::uint256, std::string>
{
    /**
     * @brief Identifier for this item in the schema dump ("uint256Hex").
     */
    static constexpr std::string_view kName = "uint256Hex";
};

/**
 * @brief Validates a uint256 hex field and yields it as a strong xrpl::uint256.
 *
 * Like LedgerHashConverter, but produces the strong type so the handler receives
 * a ready hash and never re-parses the string.
 */
struct Uint256HexConverter : HexConverterBase<xrpl::uint256, xrpl::uint256>
{
    /**
     * @brief Identifier for this item in the schema dump ("uint256Hex").
     */
    static constexpr std::string_view kName = "uint256Hex";
};

/**
 * @brief Validates a uint192 hex field and yields it as a strong xrpl::uint192.
 *
 * The uint192 form of @ref Uint256HexConverter, used for MPT issuance ids.
 */
struct Uint192HexConverter : HexConverterBase<xrpl::uint192, xrpl::uint192>
{
    /**
     * @brief Identifier for this item in the schema dump ("uint192Hex").
     */
    static constexpr std::string_view kName = "uint192Hex";
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
    /**
     * @brief Identifier for this item in the schema dump ("bool").
     */
    static constexpr std::string_view kName = "bool";

    /**
     * @brief The value this converter produces (`JsonBool`).
     */
    using ValueType = JsonBool;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if constexpr (Strict)
        {
            if (not fieldView.isBool())
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
            return JsonBool{fieldView.asBool()};
        }
        else
        {
            if (fieldView.isBool())
                return JsonBool{fieldView.asBool()};
            if (fieldView.isUint32())
                return JsonBool{fieldView.asUint32() != 0};
            if (fieldView.isInt64())
                return JsonBool{fieldView.asInt64() != 0};
            if (fieldView.isDouble())
                return JsonBool{fieldView.asDouble() != 0.0};
            if (fieldView.isString())
            {
                auto const text = fieldView.asString();
                return JsonBool{not text.empty() and text[0] != 0};
            }
            if (fieldView.isArray())
                return JsonBool{fieldView.arraySize() != 0};
            if (fieldView.isObject())
                return JsonBool{fieldView.objectSize() != 0};
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
    /**
     * @brief Identifier for this item in the schema dump ("uint32").
     */
    static constexpr std::string_view kName = "uint32";

    /**
     * @brief The value this converter produces (`uint32_t`).
     */
    using ValueType = uint32_t;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if (not fieldView.isUint32())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
        return fieldView.asUint32();
    }
};

/**
 * @brief Validates a field is a string and yields it (e.g. after a `toLower` modifier).
 */
struct StringConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("string").
     */
    static constexpr std::string_view kName = "string";

    /**
     * @brief The value this converter produces (`std::string`).
     */
    using ValueType = std::string;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if (not fieldView.isString())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
        return std::string{fieldView.asString()};
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
    /**
     * @brief Identifier for this item in the schema dump ("account").
     */
    static constexpr std::string_view kName = "account";

    /**
     * @brief The value this converter produces (`xrpl::AccountID`).
     */
    using ValueType = xrpl::AccountID;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if (fieldView.isString())
        {
            if (auto id = detail::accountFromStringStrict(std::string{fieldView.asString()});
                id.has_value())
                return *id;
        }
        return std::unexpected{rpc::Status{rpc::XrpldError::RpcActMalformed}};
    }
};

/**
 * @brief Converts an array of base58 account strings into a vector of xrpl::AccountID.
 *
 * Expects the elements to have been checked already - pair it with a validator such as
 * @ref AccountIdArrayValidator on the same field.
 */
struct AccountIdVecConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("accountIdVec").
     */
    static constexpr std::string_view kName = "accountIdVec";

    /**
     * @brief The value this converter produces (`std::optional<std::vector<xrpl::AccountID>>`).
     */
    using ValueType = std::optional<std::vector<xrpl::AccountID>>;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        auto const size = fieldView.arraySize();
        std::vector<xrpl::AccountID> result;
        result.reserve(size);
        for (auto i = 0uz; i < size; ++i)
        {
            result.push_back(
                detail::accountFromValidated(std::string{fieldView.element(i).asString()}));
        }

        return ValueType{std::move(result)};
    }
};

/**
 * @brief Converts an array of `uint192` hex strings into a vector of xrpl::uint192.
 *
 * Expects the elements to have been checked already - pair it with a validator such as
 * @ref MptIssuanceIdArrayValidator on the same field.
 */
struct Uint192VecConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("uint192Vec").
     */
    static constexpr std::string_view kName = "uint192Vec";

    /**
     * @brief The value this converter produces (`std::optional<std::vector<xrpl::uint192>>`).
     */
    using ValueType = std::optional<std::vector<xrpl::uint192>>;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        auto const size = fieldView.arraySize();
        std::vector<xrpl::uint192> result;
        result.reserve(size);
        for (auto i = 0uz; i < size; ++i)
        {
            result.push_back(
                detail::uint192FromValidated(std::string{fieldView.element(i).asString()}));
        }

        return ValueType{std::move(result)};
    }
};

/**
 * @brief How xrpld reports an account marker that is a string but does not parse.
 */
enum class MalformedMarker : std::uint8_t {
    NamesField,     ///< `invalidParams`, "Invalid field '<key>'."
    InvalidParams,  ///< `invalidParams` with no message, so xrpld's "Invalid parameters."
};

/**
 * @brief Validates an account-pagination marker (`"<uint256hex>,<uint64>"`) and yields its text.
 *
 * On xrpld the hint is read the way xrpld's handlers read it: up to the next comma, with an
 * optional leading `+`.
 *
 * @tparam OnXrpld How xrpld reports a marker that does not parse.
 */
template <MalformedMarker OnXrpld>
struct AccountMarkerStrConverterT
{
    /**
     * @brief Identifier for this item in the schema dump ("accountMarker").
     */
    static constexpr std::string_view kName = "accountMarker";

    /**
     * @brief The value this converter produces (`std::string`).
     */
    using ValueType = std::string;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams,
                kIsXrpldBuild ? rpc::expectedFieldMessage(fieldView.key(), "string")
                              : rpc::notStringFieldMessage(fieldView.key()),
            }};
        }
        auto const sv = fieldView.asString();
        auto const malformed = [&] {
            if constexpr (kIsXrpldBuild and OnXrpld == MalformedMarker::InvalidParams)
            {
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
            }
            else
            {
                return std::unexpected{rpc::Status{
                    rpc::kMalformedField,
                    rpc::malformedCursorMessage(fieldView.key()),
                }};
            }
        };
        auto const commaPos = sv.find(',');
        if (commaPos == std::string_view::npos)
            return malformed();
        auto const hexPart = std::string{sv.substr(0, commaPos)};
        auto hintPart = sv.substr(commaPos + 1);
        xrpl::uint256 index;
        if (not index.parseHex(hexPart.c_str()))
            return malformed();
        if constexpr (kIsXrpldBuild)
        {
            hintPart = hintPart.substr(0, hintPart.find(','));
            if (hintPart.starts_with('+'))
                hintPart.remove_prefix(1);
        }
        uint64_t hint = 0;
        auto const [ptr, ec] =
            std::from_chars(hintPart.data(), hintPart.data() + hintPart.size(), hint);
        if (ec != std::errc() or ptr != hintPart.data() + hintPart.size())
            return malformed();
        return std::string{sv};
    }
};

/**
 * @brief Wraps a converter so that, on xrpld, an empty string reads as an absent field.
 *
 * xrpld treats an empty optional account filter (`peer`, `destination_account`) as no filter.
 * On Clio the wrapped converter sees every value, as before.
 *
 * @tparam Conv The converter applied to any other value.
 */
template <typename Conv>
struct EmptyAsAbsentConverter
{
    /**
     * @brief Identifier for this item in the schema dump; the wrapped converter's.
     */
    static constexpr std::string_view kName = Conv::kName;

    /**
     * @brief The value this converter produces: the wrapped value, or nullopt for `""`.
     */
    using ValueType = std::optional<typename Conv::ValueType>;

    /**
     * @brief The wrapped converter.
     */
    Conv conv;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if constexpr (kIsXrpldBuild)
        {
            if (fieldView.isString() and fieldView.asString().empty())
                return ValueType{};
        }
        auto value = conv.parse(fieldView);
        if (not value.has_value())
            return std::unexpected{std::move(value).error()};
        return ValueType{std::move(value).value()};
    }
};

/**
 * @brief Wrap @p conv so that, on xrpld, an empty string reads as an absent field.
 *
 * @param conv The converter applied to any other value.
 * @return The wrapping converter.
 */
template <typename Conv>
[[nodiscard]] consteval auto
emptyAsAbsent(Conv conv)
{
    return EmptyAsAbsentConverter<Conv>{conv};
}

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: validates and decodes an account field into xrpl::AccountID with
 * per-key error messages.
 */
inline constexpr auto accountId = AccountIdConverter{};

/**
 * @brief Converter instance: validates and decodes an account field into xrpl::AccountID, mapping
 * all failures to RpcActMalformed.
 */
inline constexpr auto accountIdActMalformed = AccountIdActMalformedConverter{};

/**
 * @brief Converter instance: validates a field is a uint32 and yields it.
 */
inline constexpr auto asUint32 = Uint32Converter{};

/**
 * @brief Converter instance: validates a field is a string and yields it.
 */
inline constexpr auto asString = StringConverter{};

/**
 * @brief Converter instance: validates and decodes a hex-encoded uint256 field into a std::string.
 */
inline constexpr auto ledgerHashHex = LedgerHashConverter{};

/**
 * @brief Converter instance: validates a hex-encoded uint256 field and yields a strong
 * xrpl::uint256.
 */
inline constexpr auto asUint256 = Uint256HexConverter{};

/**
 * @brief Converter instance: validates a hex-encoded uint192 field and yields a strong
 * xrpl::uint192.
 */
inline constexpr auto asUint192 = Uint192HexConverter{};

/**
 * @brief Converter instance: decodes an array of base58 accounts into a vector of AccountID.
 */
inline constexpr auto asAccountIdVec = AccountIdVecConverter{};

/**
 * @brief Converter instance: decodes an array of uint192 hex strings into a vector of uint192.
 */
inline constexpr auto asUint192Vec = Uint192VecConverter{};

/**
 * @brief Converter instance: lenient bool converter (any JSON scalar coerced to bool; V1 API
 * semantics).
 */
inline constexpr auto jsonBool = JsonBoolConverterT<false>{};

/**
 * @brief Converter instance: strict bool converter (field must be a JSON bool; V2 API semantics).
 */
inline constexpr auto jsonBoolStrict = JsonBoolConverterT<true>{};

/**
 * @brief Converter instance: an account marker; on xrpld a malformed one names the field.
 */
inline constexpr auto accountMarkerStr = AccountMarkerStrConverterT<MalformedMarker::NamesField>{};

/**
 * @brief Converter instance: an account marker; on xrpld a malformed one is "Invalid parameters."
 */
inline constexpr auto accountMarkerStrInvalidParams =
    AccountMarkerStrConverterT<MalformedMarker::InvalidParams>{};
// NOLINTEND(readability-identifier-naming)

}  // namespace rpc::spec
