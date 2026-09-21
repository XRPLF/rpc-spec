/** @file */
#pragma once

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/tokens.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace rpc::spec::detail {

/**
 * @brief Parse a base58 token, rejecting any non-alphanumeric input first.
 *
 * @param str The base58 text to decode.
 * @return The decoded value, or nullopt.
 */
template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(std::string const& str)
{
    if (not std::ranges::all_of(str, [](unsigned char chr) { return std::isalnum(chr) != 0; }))
        return std::nullopt;
    return xrpl::parseBase58<T>(str);
}

/**
 * @brief Parse a base58 token, rejecting any non-alphanumeric input first.
 *
 * @param type The expected token type.
 * @param str The base58 text to decode.
 * @return The decoded value, or nullopt.
 */
template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(xrpl::TokenType type, std::string const& str)
{
    if (not std::ranges::all_of(str, [](unsigned char chr) { return std::isalnum(chr) != 0; }))
        return std::nullopt;
    return xrpl::parseBase58<T>(type, str);
}

/**
 * @brief Decode an account from a base58 address or a hex public key.
 *
 * @param account A base58 address or a hex public key.
 * @return The account id, or nullopt when neither form parses.
 */
[[nodiscard]] inline std::optional<xrpl::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = xrpl::strUnHex(account);
    std::optional<xrpl::PublicKey> publicKey{};
    if (blob.has_value() and xrpl::publicKeyType(xrpl::makeSlice(*blob)))
    {
        publicKey = xrpl::PublicKey(xrpl::Slice{blob->data(), blob->size()});
    }
    else
    {
        publicKey = parseBase58Wrapper<xrpl::PublicKey>(xrpl::TokenType::AccountPublic, account);
    }
    if (publicKey.has_value())
        return xrpl::calcAccountID(*publicKey);
    return parseBase58Wrapper<xrpl::AccountID>(account);
}

// "Validated extraction" helpers: convert a value the spec validator already
// confirmed into its strong type. They check the precondition (and throw if it
// is somehow violated) before dereferencing, so the conversion is total and
// clang-tidy can see the access is guarded. Use these in converters that run
// after a validator has guaranteed the input shape.

/**
 * @brief Decode a validated account string into an xrpl::AccountID.
 *
 * @param account A string a validator already accepted (base58, or a hex public key).
 * @return The decoded account id.
 * @throws std::logic_error if @p account was not in fact pre-validated.
 */
[[nodiscard]] inline xrpl::AccountID
accountFromValidated(std::string const& account)
{
    auto const id = accountFromStringStrict(account);
    if (not id.has_value())
        throw std::logic_error("accountFromValidated: account was not pre-validated");
    return *id;
}

/**
 * @brief Decode a validated hex string into an xrpl::uint256.
 *
 * @param hex A 64-character hex string a validator already accepted.
 * @return The decoded 256-bit value.
 * @throws std::logic_error if @p hex was not in fact pre-validated.
 */
[[nodiscard]] inline xrpl::uint256
uint256FromValidated(std::string const& hex)
{
    xrpl::uint256 out;
    if (not out.parseHex(hex.c_str()))
        throw std::logic_error("uint256FromValidated: hex was not pre-validated");
    return out;
}

/**
 * @brief Decode a validated hex string into an xrpl::uint192.
 *
 * @param hex A 48-character hex string a validator already accepted.
 * @return The decoded 192-bit value.
 * @throws std::logic_error if @p hex was not in fact pre-validated.
 */
[[nodiscard]] inline xrpl::uint192
uint192FromValidated(std::string const& hex)
{
    xrpl::uint192 out;
    if (not out.parseHex(hex.c_str()))
        throw std::logic_error("uint192FromValidated: hex was not pre-validated");
    return out;
}

/**
 * @brief Decode a validated currency-code string into an xrpl::Currency.
 *
 * @param code A currency code a validator already accepted.
 * @return The decoded currency.
 * @throws std::logic_error if @p code was not in fact pre-validated.
 */
[[nodiscard]] inline xrpl::Currency
currencyFromValidated(std::string const& code)
{
    xrpl::Currency out;
    if (not xrpl::toCurrency(out, code))
        throw std::logic_error("currencyFromValidated: currency was not pre-validated");
    return out;
}

/**
 * @brief Decode a validated issuer string into an xrpl::AccountID.
 *
 * @param issuer An issuer string a validator already accepted.
 * @return The decoded issuer account id.
 * @throws std::logic_error if @p issuer was not in fact pre-validated.
 */
[[nodiscard]] inline xrpl::AccountID
issuerFromValidated(std::string const& issuer)
{
    xrpl::AccountID out;
    if (not xrpl::toIssuer(out, issuer))
        throw std::logic_error("issuerFromValidated: issuer was not pre-validated");
    return out;
}

/**
 * @brief Parse a UTC timestamp against @p format.
 *
 * @param dateStr The UTC timestamp to parse.
 * @param format The strftime-style format to parse against.
 * @return The time point, or nullopt when it does not parse.
 */
[[nodiscard]] inline std::optional<std::chrono::system_clock::time_point>
systemTpFromUtcStr(std::string const& dateStr, std::string const& format)
{
    std::tm ts{};
    if (strptime(dateStr.c_str(), format.c_str(), &ts) == nullptr)
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(timegm(&ts));
}

}  // namespace rpc::spec::detail
