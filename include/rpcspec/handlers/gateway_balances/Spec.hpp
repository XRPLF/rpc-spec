/** @file */
#pragma once
// Shared constexpr spec for the 'gateway_balances' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// Two versioned specs are exposed:
//   kSpecV1 — uses RpcInvalidHotwallet for type mismatches on 'hotwallet'
//   kSpecV2 — uses RpcInvalidParams for type mismatches on 'hotwallet'

#include <rpcspec/Aliases.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/gateway_balances/Types.hpp>

#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/tokens.h>

#include <cstddef>
#include <string>

namespace rpc::spec::handlers::gateway_balances {

static constexpr auto kHOT_WALLET_V1 =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isString() && !f.isArray()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidHotwallet,
                std::string{f.key()} + "NotStringOrArray"
            }};
        }
        auto const getAccountID = [](auto const& elem) -> bool {
            if (!elem.isString())
                return false;
            auto const str = std::string{elem.asString()};
            auto const pk = detail::parseBase58Wrapper<xrpl::PublicKey>(
                xrpl::TokenType::AccountPublic, str
            );
            if (pk)
                return true;
            return detail::parseBase58Wrapper<xrpl::AccountID>(str).has_value();
        };
        if (f.isArray()) {
            for (std::size_t i = 0; i < f.arraySize(); ++i) {
                if (!getAccountID(f.element(i))) {
                    return std::unexpected{rpc::Status{
                        rpc::RippledError::RpcInvalidHotwallet,
                        std::string{f.key()} + "Malformed"
                    }};
                }
            }
        } else {
            if (!getAccountID(f)) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidHotwallet, std::string{f.key()} + "Malformed"
                }};
            }
        }
        return {};
    }};

static constexpr auto kHOT_WALLET_V2 =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isString() && !f.isArray()) {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotStringOrArray"
            }};
        }
        auto const getAccountID = [](auto const& elem) -> bool {
            if (!elem.isString())
                return false;
            auto const str = std::string{elem.asString()};
            auto const pk = detail::parseBase58Wrapper<xrpl::PublicKey>(
                xrpl::TokenType::AccountPublic, str
            );
            if (pk)
                return true;
            return detail::parseBase58Wrapper<xrpl::AccountID>(str).has_value();
        };
        if (f.isArray()) {
            for (std::size_t i = 0; i < f.arraySize(); ++i) {
                if (!getAccountID(f.element(i))) {
                    return std::unexpected{rpc::Status{
                        rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "Malformed"
                    }};
                }
            }
        } else {
            if (!getAccountID(f)) {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "Malformed"
                }};
            }
        }
        return {};
    }};

inline constexpr auto kSpecV1 = RpcSpec{
    field("account", required, account),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("hotwallet", kHOT_WALLET_V1),
};

inline constexpr auto kSpecV2 = RpcSpec{
    field("account", required, account),
    field("ledger_hash", uint256Hex),
    field("ledger_index", ledgerIndex),
    field("hotwallet", kHOT_WALLET_V2),
};

} // namespace rpc::spec::handlers::gateway_balances
