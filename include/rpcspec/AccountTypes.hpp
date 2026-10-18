/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <expected>

namespace rpc::spec {

/**
 * @brief Why a deferred account field holds no account.
 */
enum class AccountError : std::uint8_t {
    Missing,    ///< The request did not send the field, or sent a value that reads as absent.
    Malformed,  ///< The value is a string that does not decode to an account.
};

/**
 * @brief An account whose decoding failure is reported by the handler rather than the spec.
 *
 * xrpld raises `actMalformed` only after it has resolved the ledger, and with the ledger fields
 * in the response, so on xrpld a string that does not decode is carried here instead of failing
 * the parse. Clio rejects it in the spec, so on Clio a present field always holds an account.
 * A field the request may omit defaults to `std::unexpected{AccountError::Missing}`.
 */
using DeferredAccountId = std::expected<xrpl::AccountID, AccountError>;

}  // namespace rpc::spec
