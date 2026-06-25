/** @file */
#pragma once

#include <boost/json/array.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::deposit_authorized {

/**
 * @brief Input for the 'deposit_authorized' RPC command.
 */
struct Input {
  std::string sourceAccount;
  std::string destinationAccount;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  std::optional<boost::json::array> credentials;
};

} // namespace rpc::spec::handlers::deposit_authorized
