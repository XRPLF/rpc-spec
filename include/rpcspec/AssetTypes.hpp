/** @file */
#pragma once

#include <xrpl/protocol/Asset.h>

#include <rpcspec/DeferredTypes.hpp>

namespace rpc::spec {

/**
 * @brief An asset whose parsing failure is reported by the handler rather than the spec.
 *
 * xrpld raises `issueMalformed` only after it has resolved the ledger, so on xrpld a value that
 * does not describe an asset is carried as @ref DeferredError::Malformed.
 */
using DeferredAsset = Deferred<xrpl::Asset>;

}  // namespace rpc::spec
