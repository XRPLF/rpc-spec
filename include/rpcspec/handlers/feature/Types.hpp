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
struct Input
{
    /**
     * @brief The ledger selected by `ledger_hash` / `ledger_index`, or unspecified.
     */
    LedgerSpecifier ledger;

    /**
     * @brief Either an amendment name or a hex amendment id.
     *
     * An opaque passthrough with no single strong type; resolved by the (admin) handler.
     */
    std::optional<std::string> feature;
};

}  // namespace rpc::spec::handlers::feature
