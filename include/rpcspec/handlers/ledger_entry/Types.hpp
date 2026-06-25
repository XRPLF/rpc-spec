/** @file */
#pragma once

#include <boost/json/object.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/STXChainBridge.h>

#include <cstdint>
#include <optional>
#include <string>

namespace rpc::spec::handlers::ledger_entry {

/**
 * @brief Input for the 'ledger_entry' RPC command.
 */
struct Input {
    std::optional<std::string> ledgerHash;
    std::optional<uint32_t> ledgerIndex;
    bool binary = false;
    // id of this ledger entry: 256 bits hex string
    std::optional<std::string> index;
    // index can be extracted from payment_channel, check, escrow, offer
    // etc, expectedType is used to save the type of index
    xrpl::LedgerEntryType expectedType = xrpl::ltANY;
    // account id to address account root object
    std::optional<std::string> accountRoot;
    // account id to address did object
    std::optional<std::string> did;
    // mpt issuance id to address mptIssuance object
    std::optional<std::string> mptIssuance;
    // TODO: extract into custom objects, remove json from Input
    std::optional<boost::json::object> directory;
    std::optional<boost::json::object> offer;
    std::optional<boost::json::object> rippleStateAccount;
    std::optional<boost::json::object> escrow;
    std::optional<boost::json::object> depositPreauth;
    std::optional<boost::json::object> ticket;
    std::optional<boost::json::object> amm;
    std::optional<boost::json::object> mptoken;
    std::optional<boost::json::object> permissionedDomain;
    std::optional<boost::json::object> vault;
    std::optional<boost::json::object> loanBroker;
    std::optional<boost::json::object> loan;
    std::optional<xrpl::STXChainBridge> bridge;
    std::optional<std::string> bridgeAccount;
    std::optional<uint32_t> chainClaimId;
    std::optional<uint32_t> createAccountClaimId;
    std::optional<xrpl::uint256> oracleNode;
    std::optional<xrpl::uint256> credential;
    std::optional<boost::json::object> delegate;
    bool includeDeleted = false;
};

} // namespace rpc::spec::handlers::ledger_entry
