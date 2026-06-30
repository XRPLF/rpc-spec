/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_lines {

inline constexpr uint32_t kLimitMin = 10;
inline constexpr uint32_t kLimitMax = 400;
inline constexpr uint32_t kLimitDefault = 200;

/**
 * @brief Input for the 'account_lines' RPC command.
 */
struct Input {
    xrpl::AccountID account;
    LedgerSpecifier ledger;
    std::optional<xrpl::AccountID> peer;
    bool ignoreDefault = false;  // TODO: document
                                 // https://github.com/XRPLF/xrpl-dev-portal/issues/1839
    uint32_t limit = kLimitDefault;
    std::optional<std::string> marker;  /**< Opaque pagination cursor (may encode an account + hint, not a single id); re-parsed by traverseOwnedNodes downstream, so kept as a validated string. */
};

}  // namespace rpc::spec::handlers::account_lines
