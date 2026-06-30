/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>
#include <rpcspec/Ledger.hpp>

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
 * @brief Input for the 'account_tx' RPC command.
 */
struct Input {
  xrpl::AccountID account;
  // You must use at least one of the following fields in your request:
  // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
  // `ledger` is unspecified when none of ledger_hash/ledger_index is given, so
  // the handler can choose between range mode (min/max) and the default ledger.
  LedgerSpecifier ledger;
  std::optional<int32_t> ledgerIndexMin;
  std::optional<int32_t> ledgerIndexMax;
  JsonBool binary{false};
  JsonBool forward{false};
  std::optional<uint32_t> limit;
  std::optional<Marker> marker;
  std::optional<std::string> transactionTypeInLowercase;
};

} // namespace rpc::spec::handlers::account_tx
