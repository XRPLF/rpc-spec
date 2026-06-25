/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>

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
  std::string account;
  // You must use at least one of the following fields in your request:
  // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  std::optional<int32_t> ledgerIndexMin;
  std::optional<int32_t> ledgerIndexMax;
  bool usingValidatedLedger = false;
  JsonBool binary{false};
  JsonBool forward{false};
  std::optional<uint32_t> limit;
  std::optional<Marker> marker;
  std::optional<std::string> transactionTypeInLowercase;
};

} // namespace rpc::spec::handlers::account_tx
