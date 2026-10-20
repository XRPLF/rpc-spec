/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <rpcspec/DeferredTypes.hpp>

namespace rpc::spec {

/**
 * @brief An account whose decoding failure is reported by the handler rather than the spec.
 *
 * xrpld raises `actMalformed` only after it has resolved the ledger, so on xrpld a value that does
 * not decode is carried as @ref DeferredError::Malformed.
 */
using DeferredAccountId = Deferred<xrpl::AccountID>;

}  // namespace rpc::spec
