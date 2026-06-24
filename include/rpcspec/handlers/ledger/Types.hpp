/** @file */
#pragma once
// Shared Input type for the 'ledger' RPC command.
// Both Clio and rippled use this definition; server-specific fields (full, accounts,
// queue for rippled; diff for Clio) are present in the struct but irrelevant to the
// other server — the spec rejects them before the handler ever reads them.

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::ledger {

struct Input {
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t>    ledgerIndex;
    bool binary          = false;
    bool expand          = false;
    bool ownerFunds      = false;
    bool transactions    = false;
    bool diff            = false;  // Clio extension; spec rejects in rippled
    bool full            = false;  // rippled; spec rejects-if-true in Clio
    bool accounts        = false;  // rippled; spec rejects-if-true in Clio
    bool queue           = false;  // rippled; spec rejects-if-true in Clio
    // True if any ledger selector (ledger_hash, ledger_index, or deprecated
    // ledger) was present in the request. When false, the handler returns the
    // default ledger view without performing a specific ledger lookup.
    bool ledgerSpecified = false;
};

}  // namespace rpc::spec::handlers::ledger
