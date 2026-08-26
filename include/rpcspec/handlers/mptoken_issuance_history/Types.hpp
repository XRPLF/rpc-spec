/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::mptoken_issuance_history {

inline constexpr auto kLimitMin = 1;
inline constexpr auto kLimitMax = 100;
inline constexpr auto kLimitDefault = 50;

/** @brief Pagination marker for the 'mptoken_issuance_history' command. */
struct Marker
{
    uint32_t ledger;
    uint32_t seq;
};

/**
 * @brief Input for the 'mptoken_issuance_history' RPC command.
 *
 * @note Clio-only. When no ledger selector is given the request uses the backend's full
 * available ledger range, optionally narrowed by ledger_index_min/max.
 */
struct Input
{
    LedgerSpecifier ledger;
    xrpl::uint192 mptIssuanceId;
    std::optional<xrpl::AccountID> account;
    std::optional<int32_t> ledgerIndexMin;
    std::optional<int32_t> ledgerIndexMax;
    JsonBool binary{false};
    JsonBool forward{false};
    std::optional<uint32_t> limit;
    std::optional<Marker> marker;
    std::optional<std::string>
        transactionTypeInLowercase; /**< Validated tx-type name, kept as a normalized string for
                                       the same reason as account_tx: the valid set is derived at
                                       runtime from libxrpl TxFormats. */
};

}  // namespace rpc::spec::handlers::mptoken_issuance_history
