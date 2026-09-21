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

/**
 * @brief Smallest `limit` the handler accepts.
 */
inline constexpr auto kLimitMin = 1;

/**
 * @brief Largest `limit` the handler accepts; bigger values are clamped down.
 */
inline constexpr auto kLimitMax = 1000;

/**
 * @brief `limit` applied when the request omits the field.
 */
inline constexpr auto kLimitDefault = 200;

/**
 * @brief Pagination marker for the 'account_tx' command.
 */
struct Marker
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    uint32_t ledger;

    /**
     * @brief Value of the `seq` field.
     */
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
    /**
     * @brief The side of a delegated transaction the queried account is on.
     */
    enum class Role : std::uint8_t {
        /**
         * @brief The *active* sender, acting on behalf of another party.
         *
         * e.g. Account A in "A sends payment to B on behalf of C."
         */
        Actor,

        /**
         * @brief The *passive* party whose funds are moved.
         *
         * e.g. Account C in "A sends payment to B on behalf of C."
         */
        Authorizer
    };

    /**
     * @brief Value of the `delegate_type` field.
     */
    Role delegateType;

    /**
     * @brief Value of the `counter_party` field.
     */
    std::optional<std::string> counterParty;

    /**
     * @brief Compare two values of this type.
     *
     * @return The comparison result.
     */
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
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Value of the `account` request field.
     */
    xrpl::AccountID account;

    /**
     * @brief Value of the `ledger_index_min` request field.
     */
    std::optional<int32_t> ledgerIndexMin;

    /**
     * @brief Value of the `ledger_index_max` request field.
     */
    std::optional<int32_t> ledgerIndexMax;

    /**
     * @brief Value of the `binary` request field.
     */
    JsonBool binary{false};

    /**
     * @brief Value of the `forward` request field.
     */
    JsonBool forward{false};

    /**
     * @brief Value of the `limit` request field.
     */
    std::optional<uint32_t> limit;

    /**
     * @brief Value of the `marker` request field.
     */
    std::optional<Marker> marker;

    /**
     * @brief Validated tx-type name, kept as a normalized string rather than a strong enum.
     *
     * The valid set is version-dependent (derived at runtime from libxrpl TxFormats), so a
     * repo-local enum would duplicate xrpl::TxType and risk drift.
     */
    std::optional<std::string> transactionTypeInLowercase;

    /**
     * @brief Value of the `mpt_issuance_id` request field.
     */
    std::optional<xrpl::uint192> mptIssuanceId;

    /**
     * @brief Value of the `delegate` request field.
     */
    std::optional<DelegateFilter> delegateFilter;
};

}  // namespace rpc::spec::handlers::account_tx
