/** @file */
#pragma once

#include <rpcspec/Ledger.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::feature {

/**
 * @brief Input for the 'feature' RPC command.
 */
struct Input {
    LedgerSpecifier ledger;
    std::optional<std::string> feature;
};

} // namespace rpc::spec::handlers::feature
