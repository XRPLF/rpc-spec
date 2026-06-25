/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::ledger {

struct Input {
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  bool ledgerSpecified = false;

  bool binary = false;
  bool expand = false;
  bool ownerFunds = false;
  bool transactions = false;
  bool diff = false;     // Clio extension; spec rejects in rippled
  bool full = false;     // rippled; spec rejects-if-true in Clio
  bool accounts = false; // rippled; spec rejects-if-true in Clio
  bool queue = false;    // rippled; spec rejects-if-true in Clio
};

} // namespace rpc::spec::handlers::ledger
