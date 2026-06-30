/** @file */
#pragma once

#include <boost/json/array.hpp>
#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::deposit_authorized {

/**
 * @brief Input for the 'deposit_authorized' RPC command.
 */
struct Input {
  xrpl::AccountID sourceAccount;
  xrpl::AccountID destinationAccount;
  std::optional<std::string> ledgerHash;
  std::optional<uint32_t> ledgerIndex;
  std::optional<boost::json::array> credentials;
};

} // namespace rpc::spec::handlers::deposit_authorized
