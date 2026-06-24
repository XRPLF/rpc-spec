/** @file */
#pragma once
// Compatibility shim for xrpl namespace migration.
//
// Conan xrpl/3.1.x (used by Clio):  namespace ripple, error_code_i, rpcSUCCESS
// rippled source tree (newer):       namespace xrpl,   ErrorCodeI,   RpcSuccess
//
// Set RPCSPEC_IS_RIPPLED=1 (via CMake compile definition) when building
// against rippled's source tree. Without it, headers compile as usual against
// the Conan package.
//
// Always includes the xrpl protocol headers needed by rpcspec.

#if RPCSPEC_IS_RIPPLED

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/tokens.h>

// Backward-compat namespace ripple exposing the new xrpl:: types under their
// old names. A namespace alias (namespace ripple = xrpl) cannot be extended,
// so we use explicit using-declarations instead.
namespace ripple {

// Types with unchanged names
using xrpl::AccountID;
using xrpl::Currency;
using xrpl::PublicKey;
using xrpl::Slice;
using xrpl::TokenType;
using xrpl::LedgerEntryType;
using xrpl::calcAccountID;
using xrpl::isXRP;
using xrpl::makeSlice;
using xrpl::noAccount;
using xrpl::parseBase58;
using xrpl::publicKeyType;
using xrpl::strUnHex;
using xrpl::toCurrency;
using xrpl::toIssuer;

// Old name aliases (functions/constants whose names changed in the new API)
inline auto to_currency(xrpl::Currency& c, std::string const& s) { return xrpl::toCurrency(c, s); }
inline auto to_issuer(xrpl::AccountID& a, std::string const& s) { return xrpl::toIssuer(a, s); }
inline auto strViewUnHex(std::string_view s) { return xrpl::strUnHex(s); }
inline constexpr std::size_t maxCredentialTypeLength  = xrpl::kMaxCredentialTypeLength;
inline constexpr std::size_t maxCredentialsArraySize  = xrpl::kMaxCredentialsArraySize;

// Ledger entry type constants (same lt* names, just different namespace)
using xrpl::ltANY;
using xrpl::ltACCOUNT_ROOT;
using xrpl::ltAMENDMENTS;
using xrpl::ltCHECK;
using xrpl::ltDEPOSIT_PREAUTH;
using xrpl::ltDIR_NODE;
using xrpl::ltESCROW;
using xrpl::ltFEE_SETTINGS;
using xrpl::ltLEDGER_HASHES;
using xrpl::ltOFFER;
using xrpl::ltPAYCHAN;
using xrpl::ltSIGNER_LIST;
using xrpl::ltRIPPLE_STATE;
using xrpl::ltTICKET;
using xrpl::ltNFTOKEN_OFFER;
using xrpl::ltNFTOKEN_PAGE;
using xrpl::ltAMM;
using xrpl::ltBRIDGE;
using xrpl::ltXCHAIN_OWNED_CLAIM_ID;
using xrpl::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID;
using xrpl::ltDID;
using xrpl::ltORACLE;
using xrpl::ltCREDENTIAL;
using xrpl::ltVAULT;
using xrpl::ltNEGATIVE_UNL;
using xrpl::ltMPTOKEN_ISSUANCE;
using xrpl::ltMPTOKEN;
using xrpl::ltPERMISSIONED_DOMAIN;
using xrpl::ltDELEGATE;

// base_uint integer types (needed by Validators.hpp HexStringValidator)
using xrpl::uint128;
using xrpl::uint160;
using xrpl::uint192;
using xrpl::uint256;

// Error codes: both the type name and member names changed in the new API.
// We wrap xrpl::ErrorCodeI in a struct so that the old ripple::error_code_i::rpcXXX
// scoped access pattern still compiles (as static constexpr members).
struct error_code_i {
    xrpl::ErrorCodeI value{};

    constexpr error_code_i() = default;
    constexpr error_code_i(xrpl::ErrorCodeI v) noexcept : value(v) {}  // NOLINT(google-explicit-constructor)
    constexpr operator xrpl::ErrorCodeI() const noexcept { return value; }

    [[nodiscard]] constexpr bool operator==(error_code_i const&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(xrpl::ErrorCodeI rhs) const noexcept { return value == rhs; }
    [[nodiscard]] constexpr bool operator!=(error_code_i const&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(xrpl::ErrorCodeI rhs) const noexcept { return value != rhs; }

    // Old-style constant names as static members for backward compat with
    // code that uses the rpc::RippledError::rpcXXX qualified-name pattern.
    static constexpr xrpl::ErrorCodeI rpcSUCCESS       = xrpl::RpcSuccess;
    static constexpr xrpl::ErrorCodeI rpcUNKNOWN       = xrpl::RpcUnknown;
    static constexpr xrpl::ErrorCodeI rpcINVALID_PARAMS = xrpl::RpcInvalidParams;
    static constexpr xrpl::ErrorCodeI rpcACT_MALFORMED  = xrpl::RpcActMalformed;
    static constexpr xrpl::ErrorCodeI rpcNOT_SUPPORTED  = xrpl::RpcNotSupported;
};

// Namespace-level constants for code that uses ripple::rpcSUCCESS (unqualified).
inline constexpr error_code_i rpcSUCCESS{xrpl::RpcSuccess};
inline constexpr error_code_i rpcUNKNOWN{xrpl::RpcUnknown};

}  // namespace ripple

#else

#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/tokens.h>

#endif
