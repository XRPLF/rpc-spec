/** @file */
#pragma once
// Shared constexpr spec for the 'subscribe' RPC command.
// Single source of truth — Clio includes this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/subscribe/Types.hpp>

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstddef>
#include <string>
#include <unordered_set>

namespace rpc::spec::handlers::subscribe {

// Validates an array of account identifiers (base58 or hex pubkey).
// Errors mirror subscribeAccountsValidator from the old system exactly:
//   - not array → RpcInvalidParams + key + "NotArray"
//   - empty array → RpcActMalformed + key + " malformed."
//   - element not string → RpcInvalidParams + key + "'sItemNotString"
//   - element invalid account → RpcActMalformed + key + "'sItemMalformed"
static constexpr auto kSUBSCRIBE_ACCOUNTS_VALIDATOR =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isArray()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotArray"
            }};
        }
        if (f.arraySize() == 0) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcActMalformed, std::string{f.key()} + " malformed."
            }};
        }
        for (std::size_t i = 0; i < f.arraySize(); ++i) {
            auto const elem = f.element(i);
            if (!elem.isString()) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams,
                    std::string{f.key()} + "'sItemNotString"
                }};
            }
            if (!rpc::spec::detail::accountFromStringStrict(std::string{elem.asString()})) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcActMalformed,
                    std::string{f.key()} + "'sItemMalformed"
                }};
            }
        }
        return {};
    }};

// Validates the streams field: must be an array of known stream name strings.
// Errors mirror subscribeStreamValidator from the old system exactly:
//   - not array → RpcInvalidParams + key + "NotArray"
//   - element not string → RpcInvalidParams + "streamNotString"
//   - element in NOT_SUPPORT set → RpcNotSupported
//   - element not in VALID set → RpcStreamMalformed
static constexpr auto kSUBSCRIBE_STREAM_VALIDATOR =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isArray()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotArray"
            }};
        }
        static std::unordered_set<std::string> const kVALID_STREAMS = {
            "ledger",
            "transactions",
            "transactions_proposed",
            "book_changes",
            "manifests",
            "validations"
        };
        static std::unordered_set<std::string> const kNOT_SUPPORT_STREAMS = {
            "peer_status", "consensus", "server"
        };
        for (std::size_t i = 0; i < f.arraySize(); ++i) {
            auto const elem = f.element(i);
            if (!elem.isString()) {
                return std::unexpected{
                    rpc::Status{rpc::RippledError::RpcInvalidParams, "streamNotString"}
                };
            }
            auto const str = std::string{elem.asString()};
            if (kNOT_SUPPORT_STREAMS.contains(str)) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcNotSupported}};
            }
            if (!kVALID_STREAMS.contains(str)) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcStreamMalformed}};
            }
        }
        return {};
    }};

// Validates the books field: must be an array of valid book objects.
// Errors mirror the old kBOOKS_VALIDATOR lambda exactly (including all parseBook errors).
static constexpr auto kBOOKS_VALIDATOR =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isArray()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotArray"
            }};
        }
        for (std::size_t i = 0; i < f.arraySize(); ++i) {
            auto const book = f.element(i);
            if (!book.isObject()) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "ItemNotObject"
                }};
            }

            auto const bothFa = book.child("both");
            if (bothFa.present() && !bothFa.isBool()) {
                return std::unexpected{
                    rpc::Status{rpc::RippledError::RpcInvalidParams, "bothNotBool"}
                };
            }

            auto const snapshotFa = book.child("snapshot");
            if (snapshotFa.present() && !snapshotFa.isBool()) {
                return std::unexpected{
                    rpc::Status{rpc::RippledError::RpcInvalidParams, "snapshotNotBool"}
                };
            }

            auto const takerFa = book.child("taker");
            if (takerFa.present()) {
                // Mirror: meta::WithCustomError(accountValidator, RpcBadIssuer + "Issuer
                // account malformed.")
                if (!takerFa.isString() ||
                    !rpc::spec::detail::accountFromStringStrict(std::string{takerFa.asString()})) {
                    return std::unexpected{rpc::Status{
                        rpc::RippledError::RpcBadIssuer, "Issuer account malformed."
                    }};
                }
            }

            // Replicate parseBook(book.as_object()) errors inline using FA child API.
            auto const takerPaysFa = book.child("taker_pays");
            if (!takerPaysFa.present()) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Missing field 'taker_pays'"
                }};
            }
            if (!takerPaysFa.isObject()) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Field 'taker_pays' is not an object"
                }};
            }

            auto const takerGetsFa = book.child("taker_gets");
            if (!takerGetsFa.present()) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Missing field 'taker_gets'"
                }};
            }
            if (!takerGetsFa.isObject()) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "Field 'taker_gets' is not an object"
                }};
            }

            // taker_pays currency
            auto const paysCurFa = takerPaysFa.child("currency");
            if (!paysCurFa.present() || !paysCurFa.isString()) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcSrcCurMalformed}};
            }
            xrpl::Currency payCurrency;
            if (!xrpl::toCurrency(payCurrency, std::string{paysCurFa.asString()})) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcSrcCurMalformed}};
            }

            // taker_gets currency
            auto const getsCurFa = takerGetsFa.child("currency");
            if (!getsCurFa.present() || !getsCurFa.isString()) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcDstAmtMalformed}};
            }
            xrpl::Currency getCurrency;
            if (!xrpl::toCurrency(getCurrency, std::string{getsCurFa.asString()})) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcDstAmtMalformed}};
            }

            // book-level domain (mirrors parseBook): must be string if present
            auto const domainFa = book.child("domain");
            if (domainFa.present() && !domainFa.isString()) {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcDomainMalformed}};
            }

            // taker_pays issuer
            xrpl::AccountID payIssuer;
            auto const paysIssuerFa = takerPaysFa.child("issuer");
            if (paysIssuerFa.present()) {
                if (!paysIssuerFa.isString()) {
                    return std::unexpected{rpc::Status{
                        rpc::RippledError::RpcInvalidParams, "takerPaysIssuerNotString"
                    }};
                }
                if (!xrpl::toIssuer(payIssuer, std::string{paysIssuerFa.asString()})) {
                    return std::unexpected{
                        rpc::Status{rpc::RippledError::RpcSrcIsrMalformed}
                    };
                }
                if (payIssuer == xrpl::noAccount()) {
                    return std::unexpected{
                        rpc::Status{rpc::RippledError::RpcSrcIsrMalformed}
                    };
                }
            } else {
                payIssuer = xrpl::xrpAccount();
            }

            if (xrpl::isXRP(payCurrency) && !xrpl::isXRP(payIssuer)) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcSrcIsrMalformed,
                    "Unneeded field 'taker_pays.issuer' for XRP currency specification."
                }};
            }
            if (!xrpl::isXRP(payCurrency) && xrpl::isXRP(payIssuer)) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcSrcIsrMalformed,
                    "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
                }};
            }

            // taker_gets issuer
            xrpl::AccountID getIssuer;
            auto const getsIssuerFa = takerGetsFa.child("issuer");
            if (getsIssuerFa.present()) {
                if (!getsIssuerFa.isString()) {
                    return std::unexpected{rpc::Status{
                        rpc::RippledError::RpcInvalidParams,
                        "taker_gets.issuer should be string"
                    }};
                }
                if (!xrpl::toIssuer(getIssuer, std::string{getsIssuerFa.asString()})) {
                    return std::unexpected{rpc::Status{
                        rpc::RippledError::RpcDstIsrMalformed,
                        "Invalid field 'taker_gets.issuer', bad issuer."
                    }};
                }
                if (getIssuer == xrpl::noAccount()) {
                    return std::unexpected{rpc::Status{
                        rpc::RippledError::RpcDstIsrMalformed,
                        "Invalid field 'taker_gets.issuer', bad issuer account one."
                    }};
                }
            } else {
                getIssuer = xrpl::xrpAccount();
            }

            if (xrpl::isXRP(getCurrency) && !xrpl::isXRP(getIssuer)) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcDstIsrMalformed,
                    "Unneeded field 'taker_gets.issuer' for XRP currency specification."
                }};
            }
            if (!xrpl::isXRP(getCurrency) && xrpl::isXRP(getIssuer)) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcDstIsrMalformed,
                    "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
                }};
            }

            if (payCurrency == getCurrency && payIssuer == getIssuer) {
                return std::unexpected{
                    rpc::Status{rpc::RippledError::RpcBadMarket, "badMarket"}
                };
            }

            // book-level domain (mirrors inner parseBook overload): must parse as hex
            if (domainFa.present()) {
                xrpl::uint256 dom;
                if (!dom.parseHex(std::string{domainFa.asString()})) {
                    return std::unexpected{rpc::Status{rpc::RippledError::RpcDomainMalformed}};
                }
            }
        }
        return {};
    }};

inline constexpr auto kSpec = RpcSpec{
    field("streams") | kSUBSCRIBE_STREAM_VALIDATOR,
    field("accounts") | kSUBSCRIBE_ACCOUNTS_VALIDATOR,
    field("accounts_proposed") | kSUBSCRIBE_ACCOUNTS_VALIDATOR,
    field("books") | kBOOKS_VALIDATOR,
    field("user") | deprecated,
    field("password") | deprecated,
    field("rt_accounts") | deprecated
};

} // namespace rpc::spec::handlers::subscribe
