/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::vault_info {

/**
 * @brief Input for the 'vault_info' RPC command.
 */
struct Input {
    std::optional<std::string> vaultID;
    std::optional<std::string> owner;
    std::optional<uint32_t> tnxSequence;
    std::optional<uint32_t> ledgerIndex;
};

} // namespace rpc::spec::handlers::vault_info
