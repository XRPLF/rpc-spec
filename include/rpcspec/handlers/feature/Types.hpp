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
    std::optional<std::string> feature;  /**< Either an amendment name or a hex amendment id — an opaque passthrough with no single strong type; resolved by the (admin) handler. */
};

} // namespace rpc::spec::handlers::feature
