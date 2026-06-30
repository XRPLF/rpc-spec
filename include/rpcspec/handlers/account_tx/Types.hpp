/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>

#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_tx {

inline constexpr auto kLimitMin = 1;
inline constexpr auto kLimitMax = 1000;
inline constexpr auto kLimitDefault = 200;

/** @brief Pagination marker for the 'account_tx' command. */
struct Marker {
  uint32_t ledger;
  uint32_t seq;
};

/**
 * @brief The resolved `ledger_index` specifier.
 *
 * A single strong member that captures the three states the legacy code spread
 * across an optional<uint32_t> plus a separate `usingValidatedLedger` bool:
 *   - a concrete numeric index (`index` set);
 *   - a sentinel ("validated"/"current"/"closed") or an unresolvable value
 *     (`index` empty, `usingValidated` true) — handler falls back to the latest;
 *   - absent (`index` empty, `usingValidated` false).
 */
struct LedgerIndexSpec {
  std::optional<uint32_t> index;
  bool usingValidated = false;
};

/**
 * @brief Input for the 'account_tx' RPC command.
 */
struct Input {
  xrpl::AccountID account;
  // You must use at least one of the following fields in your request:
  // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
  std::optional<std::string> ledgerHash;
  LedgerIndexSpec ledgerIndex;
  std::optional<int32_t> ledgerIndexMin;
  std::optional<int32_t> ledgerIndexMax;
  JsonBool binary{false};
  JsonBool forward{false};
  std::optional<uint32_t> limit;
  std::optional<Marker> marker;
  std::optional<std::string> transactionTypeInLowercase;
};

} // namespace rpc::spec::handlers::account_tx
