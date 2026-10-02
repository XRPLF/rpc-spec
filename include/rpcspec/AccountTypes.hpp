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
    Malformed  ///< The value is a string that does not decode to an account.
};

/**
 * @brief An account whose decoding failure is reported by the handler rather than the spec.
 *
 * xrpld raises `actMalformed` only after it has resolved the ledger, and with the ledger fields
 * in the response, so on xrpld a string that does not decode is carried here instead of failing
 * the parse. Clio rejects it in the spec, so on Clio this always holds an account.
 */
using DeferredAccountId = std::expected<xrpl::AccountID, AccountError>;

}  // namespace rpc::spec
