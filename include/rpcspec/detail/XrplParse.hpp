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

template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(std::string const& str)
{
    if (!std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;
    return xrpl::parseBase58<T>(str);
}

template <class T>
[[nodiscard]] std::optional<T>
parseBase58Wrapper(xrpl::TokenType type, std::string const& str)
{
    if (!std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isalnum(c); }))
        return std::nullopt;
    return xrpl::parseBase58<T>(type, str);
}

[[nodiscard]] inline std::optional<xrpl::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = xrpl::strUnHex(account);
    std::optional<xrpl::PublicKey> publicKey{};
    if (blob && xrpl::publicKeyType(xrpl::makeSlice(*blob)))
    {
        publicKey = xrpl::PublicKey(xrpl::Slice{blob->data(), blob->size()});
    }
    else
    {
        publicKey = parseBase58Wrapper<xrpl::PublicKey>(xrpl::TokenType::AccountPublic, account);
    }
    if (publicKey)
        return xrpl::calcAccountID(*publicKey);
    return parseBase58Wrapper<xrpl::AccountID>(account);
}

// "Validated extraction" helpers: convert a value the spec validator already
// confirmed into its strong type. They check the precondition (and throw if it
// is somehow violated) before dereferencing, so the conversion is total and
// clang-tidy can see the access is guarded. Use these in converters that run
// after a validator has guaranteed the input shape.

/** @brief Decode a validated account string into an xrpl::AccountID. */
[[nodiscard]] inline xrpl::AccountID
accountFromValidated(std::string const& account)
{
    auto const id = accountFromStringStrict(account);
    if (!id)
        throw std::logic_error("accountFromValidated: account was not pre-validated");
    return *id;
}

/** @brief Decode a validated hex string into an xrpl::uint256. */
[[nodiscard]] inline xrpl::uint256
uint256FromValidated(std::string const& hex)
{
    xrpl::uint256 out;
    if (!out.parseHex(hex.c_str()))
        throw std::logic_error("uint256FromValidated: hex was not pre-validated");
    return out;
}

/** @brief Decode a validated hex string into an xrpl::uint192. */
[[nodiscard]] inline xrpl::uint192
uint192FromValidated(std::string const& hex)
{
    xrpl::uint192 out;
    if (!out.parseHex(hex.c_str()))
        throw std::logic_error("uint192FromValidated: hex was not pre-validated");
    return out;
}

/** @brief Decode a validated currency-code string into an xrpl::Currency. */
[[nodiscard]] inline xrpl::Currency
currencyFromValidated(std::string const& code)
{
    xrpl::Currency out;
    if (!xrpl::toCurrency(out, code))
        throw std::logic_error("currencyFromValidated: currency was not pre-validated");
    return out;
}

/** @brief Decode a validated issuer string into an xrpl::AccountID. */
[[nodiscard]] inline xrpl::AccountID
issuerFromValidated(std::string const& issuer)
{
    xrpl::AccountID out;
    if (!xrpl::toIssuer(out, issuer))
        throw std::logic_error("issuerFromValidated: issuer was not pre-validated");
    return out;
}

[[nodiscard]] inline std::optional<std::chrono::system_clock::time_point>
systemTpFromUtcStr(std::string const& dateStr, std::string const& format)
{
    std::tm ts{};
    if (strptime(dateStr.c_str(), format.c_str(), &ts) == nullptr)
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(timegm(&ts));
}

}  // namespace rpc::spec::detail
