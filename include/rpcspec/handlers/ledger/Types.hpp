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
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `binary` request field.
     */
    JsonBool binary{false};

    /**
     * @brief Value of the `expand` request field.
     */
    JsonBool expand{false};

    /**
     * @brief Value of the `owner_funds` request field.
     */
    JsonBool ownerFunds{false};

    /**
     * @brief Value of the `transactions` request field.
     */
    JsonBool transactions{false};

    /**
     * @brief Value of the `diff` request field.
     */
    JsonBool diff{false};  // Clio extension; validate-only (ifServerClio) in xrpld

    /**
     * @brief Value of the `full` request field.
     */
    JsonBool full{false};  // xrpld; spec rejects-if-true in Clio

    /**
     * @brief Value of the `accounts` request field.
     */
    JsonBool accounts{false};  // xrpld; spec rejects-if-true in Clio

    /**
     * @brief Value of the `queue` request field.
     */
    JsonBool queue{false};  // xrpld; spec rejects-if-true in Clio
};

}  // namespace rpc::spec::handlers::ledger
