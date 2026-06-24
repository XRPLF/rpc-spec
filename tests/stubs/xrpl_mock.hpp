/** @file */
#pragma once
// Minimal mock of the libxrpl protocol surface that rpcspec's detail/XrplNs.hpp
// (rippled backend) references. This lets the standalone unit tests compile and
// run with ZERO dependency on libxrpl — only Boost::json is needed.
//
// It mirrors just enough of namespace xrpl to satisfy the `using xrpl::...`
// declarations and the inline wrappers in XrplNs.hpp / XrplParse.hpp. The bodies
// are intentionally trivial: the tests exercise the consteval DSL machinery and
// the ledger-types table, not real base58/account parsing.
//
// The eight <xrpl/...> headers that XrplNs.hpp includes are thin shims that each
// include this file (see tests/stubs/xrpl/...).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xrpl {

// ---- basics/base_uint.h -----------------------------------------------------
template <std::size_t Bits>
struct base_uint {
    std::uint8_t data_[Bits / 8]{};
};
using uint128 = base_uint<128>;
using uint160 = base_uint<160>;
using uint192 = base_uint<192>;
using uint256 = base_uint<256>;

// ---- basics/Slice.h ---------------------------------------------------------
class Slice {
    std::uint8_t const* data_ = nullptr;
    std::size_t size_ = 0;

public:
    Slice() = default;
    Slice(void const* p, std::size_t n) : data_(static_cast<std::uint8_t const*>(p)), size_(n) {}

    [[nodiscard]] std::uint8_t const* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
};

template <class Container>
[[nodiscard]] inline Slice
makeSlice(Container const& c)
{
    return Slice{c.data(), c.size()};
}

// ---- basics/StringUtilities.h -----------------------------------------------
using Blob = std::vector<unsigned char>;

[[nodiscard]] inline std::optional<Blob>
strUnHex(std::string_view)
{
    return std::nullopt;
}

// ---- protocol/tokens.h ------------------------------------------------------
enum class TokenType {
    None = 0,
    AccountID = 0,
    AccountPublic = 35,
};

// ---- protocol/AccountID.h (+ Currency) --------------------------------------
class Currency {};
class AccountID {};

[[nodiscard]] inline AccountID
noAccount()
{
    return {};
}

[[nodiscard]] inline bool
isXRP(Currency const&)
{
    return false;
}

[[nodiscard]] inline bool
toCurrency(Currency&, std::string const&)
{
    return false;
}

[[nodiscard]] inline bool
toIssuer(AccountID&, std::string const&)
{
    return false;
}

// ---- protocol/PublicKey.h ---------------------------------------------------
class PublicKey {
public:
    PublicKey() = default;
    explicit PublicKey(Slice const&) {}
};

[[nodiscard]] inline AccountID
calcAccountID(PublicKey const&)
{
    return {};
}

enum class KeyType { secp256k1, ed25519 };

[[nodiscard]] inline std::optional<KeyType>
publicKeyType(Slice const&)
{
    return std::nullopt;
}

// parseBase58 overloads used by XrplParse.hpp's wrappers.
template <class T>
[[nodiscard]] std::optional<T>
parseBase58(std::string const&)
{
    return std::nullopt;
}

template <class T>
[[nodiscard]] std::optional<T>
parseBase58(TokenType, std::string const&)
{
    return std::nullopt;
}

// Credential limits referenced by XrplNs.hpp.
inline constexpr std::size_t kMaxCredentialTypeLength = 64;
inline constexpr std::size_t kMaxCredentialsArraySize = 8;

// ---- protocol/ErrorCodes.h --------------------------------------------------
// Unscoped enum so xrpl::RpcSuccess etc. are namespace-scope constants of type
// ErrorCodeI, matching how XrplNs.hpp consumes them.
enum ErrorCodeI : int {
    RpcSuccess = 0,
    RpcUnknown = -1,
    RpcInvalidParams = 31,
    RpcActMalformed = 35,
    RpcNotSupported = 75,
};

// ---- protocol/LedgerFormats.h -----------------------------------------------
// Unscoped enum so xrpl::ltACCOUNT_ROOT etc. work and can be re-exported.
enum LedgerEntryType : int {
    ltANY = -3,
    ltACCOUNT_ROOT = 1,
    ltAMENDMENTS,
    ltCHECK,
    ltDEPOSIT_PREAUTH,
    ltDIR_NODE,
    ltESCROW,
    ltFEE_SETTINGS,
    ltLEDGER_HASHES,
    ltOFFER,
    ltPAYCHAN,
    ltSIGNER_LIST,
    ltRIPPLE_STATE,
    ltTICKET,
    ltNFTOKEN_OFFER,
    ltNFTOKEN_PAGE,
    ltAMM,
    ltBRIDGE,
    ltXCHAIN_OWNED_CLAIM_ID,
    ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID,
    ltDID,
    ltORACLE,
    ltCREDENTIAL,
    ltVAULT,
    ltNEGATIVE_UNL,
    ltMPTOKEN_ISSUANCE,
    ltMPTOKEN,
    ltPERMISSIONED_DOMAIN,
    ltDELEGATE,
};

}  // namespace xrpl
