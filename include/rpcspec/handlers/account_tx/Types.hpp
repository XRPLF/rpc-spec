/** @file */
#pragma once

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_tx {

inline constexpr auto kLimitMin = 1;
inline constexpr auto kLimitMax = 1000;
inline constexpr auto kLimitDefault = 200;

/** @brief Pagination marker for the 'account_tx' command. */
struct Marker
{
    uint32_t ledger;
    uint32_t seq;
};

/**
 * @brief Delegation filter for the 'account_tx' command.
 *
 * Selects transactions where the queried account acted in the given role, optionally
 * narrowed to a single counter party.
 */
struct DelegateFilter
{
    /** @brief The side of a delegated transaction the queried account is on. */
    enum class Role : std::uint8_t {
        Actor,     /**< The *active* sender, acting on behalf of another party.
                    * e.g. Account A in "A sends payment to B on behalf of C." */
        Authorizer /**< The *passive* party whose funds are moved.
                    * e.g. Account C in "A sends payment to B on behalf of C." */
    };

    Role delegateType;
    std::optional<std::string> counterParty;

    bool
    operator==(DelegateFilter const&) const = default;
};

/**
 * @brief Input for the 'account_tx' RPC command.
 */
struct Input
{
    // You must use at least one of the following fields in your request:
    // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
    // `ledger` is unspecified when none of ledger_hash/ledger_index is given, so
    // the handler can choose between range mode (min/max) and the default ledger.
    LedgerSpecifier ledger;
    xrpl::AccountID account;
    std::optional<int32_t> ledgerIndexMin;
    std::optional<int32_t> ledgerIndexMax;
    JsonBool binary{false};
    JsonBool forward{false};
    std::optional<uint32_t> limit;
    std::optional<Marker> marker;
    std::optional<std::string>
        transactionTypeInLowercase; /**< Validated tx-type name, kept as a normalized string rather
                                       than a strong enum: the valid set is version-dependent
                                       (derived at runtime from libxrpl TxFormats), so a repo-local
                                       enum would duplicate xrpl::TxType and risk drift. */
    std::optional<xrpl::uint192> mptIssuanceId;
    std::optional<DelegateFilter> delegateFilter;
};

}  // namespace rpc::spec::handlers::account_tx
