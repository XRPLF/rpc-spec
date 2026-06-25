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
inline constexpr auto required = Required{};
inline constexpr auto deprecated = Deprecated{};
inline constexpr auto account = AccountFormat{};

template <typename... Ts> inline constexpr auto type = Type<Ts...>{};
// NOLINTEND(readability-identifier-naming)

template <typename T> consteval auto min(T v) { return Min{v}; }
template <typename T> consteval auto clamp(T lo, T hi) { return Clamp{lo, hi}; }

// NOLINTNEXTLINE(readability-identifier-naming)
template <typename Target> inline constexpr auto clampAs = ClampAs<Target>{};

template <typename T, SomeProcessor... SubItems>
consteval auto ifType(SubItems... items) {
  return IfType<T, SubItems...>{items...};
}

template <typename Wrapped>
consteval auto withCustomError(Wrapped w, rpc::CombinedError code,
                               std::string_view message = {}) {
  return WithCustomError<Wrapped>{w, code, message};
}

consteval auto timeFormat(std::string_view format) {
  return TimeFormatValidator{format};
}

template <typename... SubFields> consteval auto section(SubFields... sf) {
  return Section<SubFields...>{sf...};
}

template <typename... Vs> consteval auto ifServerClio(Vs... vs) {
  return IfServerClioValidator<Vs...>{vs...};
}

template <typename... Vs> consteval auto ifServerRippled(Vs... vs) {
  return IfServerRippledValidator<Vs...>{vs...};
}

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto ledgerIndex = LedgerIndexValidator{};
inline constexpr auto accountBase58 = AccountBase58Validator{};
inline constexpr auto currency = CurrencyValidator{};
inline constexpr auto issuer = IssuerValidator{};
inline constexpr auto currencyIssue = CurrencyIssueValidator{};
inline constexpr auto credentialType = CredentialTypeValidator{};
inline constexpr auto authorizeCredential = AuthorizeCredentialValidator{};
inline constexpr auto toNumber = ToNumberModifier{};
inline constexpr auto uint256Hex = Uint256HexStringValidator{};
inline constexpr auto uint192Hex = Uint192HexStringValidator{};
inline constexpr auto uint160Hex = Uint160HexStringValidator{};
inline constexpr auto notSupported = NotSupported{};

template <typename T> consteval auto notSupportedIf(T value) {
  return NotSupportedIfEqual{value};
}

inline constexpr auto toLower = ToLowerModifier{};
inline constexpr auto hex256Array = Hex256ArrayValidator{};
inline constexpr auto accountMarker = AccountMarkerValidator{};
inline constexpr auto accountType = AccountTypeValidator{};
inline constexpr auto ledgerType = LedgerEntryTypeValidator{};
// NOLINTEND(readability-identifier-naming)

template <typename T = std::string, typename... Strings>
consteval auto oneOf(Strings... vals) {
  return OneOfValidator<sizeof...(Strings)>{
      std::array<std::string_view, sizeof...(Strings)>{
          std::string_view{vals}...}};
}

template <typename T> consteval auto between(T lo, T hi) {
  return Between{lo, hi};
}

template <typename Fn> consteval auto customModifier(Fn f) {
  return CustomModifier<Fn>{f};
}

} // namespace rpc::spec
