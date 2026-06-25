/** @file */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::feature {

/**
 * @brief Input for the 'feature' RPC command.
 */
struct Input {
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    std::optional<std::string> feature;
};

} // namespace rpc::spec::handlers::feature
