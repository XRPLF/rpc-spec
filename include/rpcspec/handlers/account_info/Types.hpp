/** @file */
#pragma once

#include <rpcspec/JsonBool.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::account_info {

/**
 * @brief Input for the 'account_info' RPC command.
 *
 * `queue` is not available in Reporting mode
 * `ident` is deprecated, keep it for now, in line with rippled
 */
struct Input {
    std::optional<std::string> account;
    std::optional<std::string> ident;
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    JsonBool signerLists{false};
};

}  // namespace rpc::spec::handlers::account_info
