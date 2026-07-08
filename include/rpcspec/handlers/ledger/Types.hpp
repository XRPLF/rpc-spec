/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::ledger {

/**
 * @brief Input for the 'ledger' RPC command.
 */
struct Input
{
    LedgerSpecifier ledger;
    JsonBool binary{false};
    JsonBool expand{false};
    JsonBool ownerFunds{false};
    JsonBool transactions{false};
    JsonBool diff{false};      // Clio extension; validate-only (ifServerClio) in xrpld
    JsonBool full{false};      // xrpld; spec rejects-if-true in Clio
    JsonBool accounts{false};  // xrpld; spec rejects-if-true in Clio
    JsonBool queue{false};     // xrpld; spec rejects-if-true in Clio
};

}  // namespace rpc::spec::handlers::ledger
