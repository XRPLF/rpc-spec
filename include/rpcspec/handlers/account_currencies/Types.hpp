/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_currencies {

/**
 * @brief Input for the 'account_currencies' RPC command.
 */
struct Input {
    xrpl::AccountID account;
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
};

}  // namespace rpc::spec::handlers::account_currencies
