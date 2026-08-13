/** @file */
#pragma once
// Minimal mock of the libxrpl protocol surface that rpcspec's headers (xrpld
// backend) reference. This lets the standalone unit tests compile and run with
// ZERO dependency on libxrpl — only Boost::json is needed.
//
// It mirrors just enough of namespace xrpl to satisfy the `using xrpl::...`
// declarations and the inline wrappers in XrplParse.hpp / Validators.hpp.
//
// FIDELITY NOTE: the parsers below are *structural*, not cryptographic. base58
// decoding is real (Ripple alphabet, big-number algorithm, version-byte and
// length checks) but the 4-byte SHA-256 checksum is NOT verified — pulling in a
// real hash would defeat the dependency-free design. This is enough to make the
// spec tests' fixtures behave correctly (a well-formed account/seed decodes and
// validates; a malformed or wrong-length one is rejected), but it is not a
// drop-in for libxrpl's parsing. Currency/hex parsing is likewise format-based.
//
// The <xrpl/...> headers the framework includes are thin shims that each include
// this file (see tests/stubs/xrpl/...).

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xrpl {

// ---- hex / base58 helpers (mock-local) --------------------------------------
namespace mock_detail {

[[nodiscard]] inline int
hexVal(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Decode a hex string into bytes. Returns nullopt on odd length or non-hex char.
[[nodiscard]] inline std::optional<std::vector<unsigned char>>
hexToBytes(std::string_view sv)
{
    if (sv.size() % 2 != 0)
        return std::nullopt;
    std::vector<unsigned char> out;
    out.reserve(sv.size() / 2);
    for (std::size_t i = 0; i < sv.size(); i += 2)
    {
        int const hi = hexVal(static_cast<unsigned char>(sv[i]));
        int const lo = hexVal(static_cast<unsigned char>(sv[i + 1]));
        if (hi < 0 || lo < 0)
            return std::nullopt;
        out.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return out;
}

// Ripple base58 alphabet (note: excludes 0 O I l).
inline constexpr std::string_view kBASE58_ALPHABET =
    "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz";

// Big-endian byte decode of a Ripple-base58 string. Returns nullopt on any
// character outside the alphabet. Checksum is NOT validated (see fidelity note).
[[nodiscard]] inline std::optional<std::vector<unsigned char>>
decodeBase58(std::string_view s)
{
    if (s.empty())
        return std::nullopt;
    std::vector<unsigned char> bytes;  // little-endian during accumulation
    for (char const ch : s)
    {
        auto const pos = kBASE58_ALPHABET.find(ch);
        if (pos == std::string_view::npos)
            return std::nullopt;
        int carry = static_cast<int>(pos);
        for (auto& b : bytes)
        {
            carry += 58 * b;
            b = static_cast<unsigned char>(carry & 0xff);
            carry >>= 8;
        }
        while (carry > 0)
        {
            bytes.push_back(static_cast<unsigned char>(carry & 0xff));
            carry >>= 8;
        }
    }
    // Each leading alphabet[0] char ('r') maps to a leading zero byte.
    for (char const ch : s)
    {
        if (ch != kBASE58_ALPHABET[0])
            break;
        bytes.push_back(0);
    }
    std::reverse(bytes.begin(), bytes.end());
    return bytes;
}

}  // namespace mock_detail

// ---- basics/base_uint.h -----------------------------------------------------
template <std::size_t Bits>
struct base_uint
{
    std::uint8_t data_[Bits / 8]{};

    [[nodiscard]] bool
    isZero() const noexcept
    {
        return std::all_of(std::begin(data_), std::end(data_), [](auto b) { return b == 0; });
    }

    // Parse exactly Bits/4 hex characters into the raw bytes. Returns false on
    // wrong length or non-hex input (mirrors libxrpl's strict parseHex, which
    // accepts both const char* and string_view).
    [[nodiscard]] bool
    parseHex(std::string_view sv)
    {
        if (sv.size() != Bits / 4)
            return false;
        auto const bytes = mock_detail::hexToBytes(sv);
        if (!bytes || bytes->size() != Bits / 8)
            return false;
        std::copy(bytes->begin(), bytes->end(), std::begin(data_));
        return true;
    }

    [[nodiscard]] bool
    parseHex(char const* str)
    {
        return parseHex(std::string_view{str});
    }

    bool
    operator==(base_uint const& other) const noexcept
    {
        return std::equal(std::begin(data_), std::end(data_), std::begin(other.data_));
    }
};
using uint128 = base_uint<128>;
using uint160 = base_uint<160>;
using uint192 = base_uint<192>;
using uint256 = base_uint<256>;

// ---- basics/Slice.h ---------------------------------------------------------
class Slice
{
    std::uint8_t const* data_ = nullptr;
    std::size_t size_ = 0;

public:
    Slice() = default;
    Slice(void const* p, std::size_t n) : data_(static_cast<std::uint8_t const*>(p)), size_(n)
    {
    }

    [[nodiscard]] std::uint8_t const*
    data() const noexcept
    {
        return data_;
    }
    [[nodiscard]] std::size_t
    size() const noexcept
    {
        return size_;
    }
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
strUnHex(std::string_view sv)
{
    return mock_detail::hexToBytes(sv);
}

[[nodiscard]] inline std::optional<Blob>
strViewUnHex(std::string_view sv)
{
    return mock_detail::hexToBytes(sv);
}

// ---- protocol/tokens.h ------------------------------------------------------
// Version bytes for the Ripple base58 token types we care about.
enum class TokenType {
    None = 1,
    NodePublic = 28,
    NodePrivate = 32,
    AccountID = 0,
    AccountPublic = 35,
    AccountSecret = 34,
    FamilySeed = 33,
};

// ---- protocol/AccountID.h (+ Currency) --------------------------------------
class AccountID
{
    std::array<std::uint8_t, 20> data_{};

public:
    AccountID() = default;

    [[nodiscard]] std::uint8_t*
    data() noexcept
    {
        return data_.data();
    }
    [[nodiscard]] std::uint8_t const*
    data() const noexcept
    {
        return data_.data();
    }
    [[nodiscard]] static constexpr std::size_t
    size() noexcept
    {
        return 20;
    }

    [[nodiscard]] bool
    isZero() const noexcept
    {
        return std::all_of(data_.begin(), data_.end(), [](auto b) { return b == 0; });
    }

    bool
    operator==(AccountID const& other) const noexcept = default;
};

class Currency
{
    bool isXrp_ = false;

public:
    Currency() = default;
    explicit Currency(bool isXrp) : isXrp_(isXrp)
    {
    }
    [[nodiscard]] bool
    isXrp() const noexcept
    {
        return isXrp_;
    }
    bool
    operator==(Currency const&) const noexcept = default;
};

[[nodiscard]] inline AccountID
noAccount()
{
    return {};
}

[[nodiscard]] inline AccountID
xrpAccount()
{
    return {};
}

// ---- protocol/Issue.h -------------------------------------------------------
struct Issue
{
    Currency currency;
    AccountID account;
    bool
    operator==(Issue const&) const noexcept = default;
};

[[nodiscard]] inline Issue
noIssue()
{
    return {};
}

// Parses an issue from a "CUR" / "CUR.issuer" style string. The mock accepts
// anything; the real libxrpl throws std::runtime_error on malformed input.
[[nodiscard]] inline Issue
issueFromJson(std::string const&)
{
    return {};
}

// ---- protocol/STXChainBridge.h ----------------------------------------------
struct STXChainBridge
{
    bool
    operator==(STXChainBridge const&) const noexcept = default;
};

// ---- protocol/Book.h --------------------------------------------------------
struct Book
{
    Issue in;
    Issue out;
    std::optional<uint256> domain;  // libxrpl's Book carries an optional permissioned-domain id
    bool
    operator==(Book const&) const noexcept = default;
};

[[nodiscard]] inline bool
isXRP(Currency const& c)
{
    return c.isXrp();
}

// libxrpl also has isXRP(AccountID): the XRP "account" is the all-zero ID.
[[nodiscard]] inline bool
isXRP(AccountID const& a)
{
    return a.isZero();
}

// "XRP", any 3-character ISO code, or a 40-char hex code is accepted.
[[nodiscard]] inline bool
toCurrency(Currency& currency, std::string const& code)
{
    if (code == "XRP")
    {
        currency = Currency{true};
        return true;
    }
    if (code.size() == 3)
    {
        currency = Currency{false};
        return true;
    }
    if (code.size() == 40 && mock_detail::hexToBytes(code).has_value())
    {
        currency = Currency{false};
        return true;
    }
    return false;
}

// ---- protocol/PublicKey.h ---------------------------------------------------
class PublicKey
{
public:
    PublicKey() = default;
    explicit PublicKey(Slice const&)
    {
    }
};

[[nodiscard]] inline AccountID
calcAccountID(PublicKey const&)
{
    // Non-zero so a parsed public key yields a valid-looking account.
    AccountID a;
    a.data()[0] = 1;
    return a;
}

enum class KeyType { secp256k1, ed25519 };

[[nodiscard]] inline std::optional<KeyType>
publicKeyType(Slice const& s)
{
    // libxrpl recognises 33-byte (secp256k1) and 32-byte (ed25519, 0xED-prefixed)
    // keys; for the mock we only need the size gate the wrappers rely on.
    if (s.size() == 33)
        return KeyType::secp256k1;
    return std::nullopt;
}

// parseBase58 — version-byte + length aware. Used by XrplParse.hpp's wrappers.
// Primary template: unsupported types never parse.
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

// AccountID: 1 version byte (TokenType::AccountID == 0) + 20 bytes + 4 checksum.
template <>
[[nodiscard]] inline std::optional<AccountID>
parseBase58<AccountID>(std::string const& str)
{
    auto const bytes = mock_detail::decodeBase58(str);
    if (!bytes || bytes->size() != 25)
        return std::nullopt;
    if ((*bytes)[0] != static_cast<std::uint8_t>(TokenType::AccountID))
        return std::nullopt;
    AccountID a;
    std::copy(bytes->begin() + 1, bytes->begin() + 21, a.data());
    return a;
}

// PublicKey (account-public): 1 version byte + 33 key bytes + 4 checksum.
template <>
[[nodiscard]] inline std::optional<PublicKey>
parseBase58<PublicKey>(TokenType type, std::string const& str)
{
    auto const bytes = mock_detail::decodeBase58(str);
    if (!bytes || bytes->size() != 38)
        return std::nullopt;
    if ((*bytes)[0] != static_cast<std::uint8_t>(type))
        return std::nullopt;
    return PublicKey{Slice{bytes->data() + 1, 33}};
}

[[nodiscard]] inline bool
toIssuer(AccountID& issuer, std::string const& str)
{
    auto const parsed = parseBase58<AccountID>(str);
    if (!parsed)
        return false;
    issuer = *parsed;
    return true;
}

// Credential limits referenced by Validators.hpp.
inline constexpr std::size_t kMaxCredentialTypeLength = 64;
inline constexpr std::size_t kMaxCredentialsArraySize = 8;

// ---- protocol/ErrorCodes.h --------------------------------------------------
// Unscoped enum so xrpl::RpcSuccess etc. are namespace-scope constants of type
// ErrorCodeI, matching how the framework consumes them.
enum ErrorCodeI : int {
    RpcSuccess = 0,
    RpcUnknown = -1,
    RpcInvalidParams = 31,
    RpcActMalformed = 35,
    RpcNotSupported = 75,
    // Additional codes referenced by migrated handler specs (values are
    // placeholders — only the enumerators need to exist for these mocked tests).
    RpcNoPermission = 100,
    RpcIssueMalformed = 101,
    RpcDomainMalformed = 102,
    RpcDstAmtMalformed = 103,
    RpcDstIsrMalformed = 104,
    RpcSrcCurMalformed = 105,
    RpcSrcIsrMalformed = 106,
    RpcOracleMalformed = 107,
    RpcInvalidHotwallet = 108,
    RpcStreamMalformed = 109,
    RpcBadIssuer = 110,
    RpcBadMarket = 111,
};

// ---- protocol/TxFormats.h ---------------------------------------------------
// Mock of the iterable TxFormats registry. Real libxrpl derives this from the
// linked xrpld version; the mock carries a small representative sample so
// txTypesInLowercase() yields a non-empty set.
class TxFormats
{
public:
    struct Item
    {
        std::string name_;
        [[nodiscard]] std::string const&
        getName() const noexcept
        {
            return name_;
        }
    };

    [[nodiscard]] static TxFormats const&
    getInstance()
    {
        static TxFormats const kINSTANCE{};
        return kINSTANCE;
    }

    [[nodiscard]] auto
    begin() const noexcept
    {
        return items_.begin();
    }
    [[nodiscard]] auto
    end() const noexcept
    {
        return items_.end();
    }

private:
    std::vector<Item> items_{{"Payment"}, {"OfferCreate"}, {"OfferCancel"}, {"AccountSet"}};
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
    ltLOAN_BROKER,
    ltLOAN,
    ltNEGATIVE_UNL,
    ltMPTOKEN_ISSUANCE,
    ltMPTOKEN,
    ltPERMISSIONED_DOMAIN,
    ltDELEGATE,
};

}  // namespace xrpl
