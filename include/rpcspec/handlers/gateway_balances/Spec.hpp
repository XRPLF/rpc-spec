/** @file */
#pragma once
// Shared constexpr spec for the 'gateway_balances' RPC command.
// Single source of truth — both Clio and rippled include this file.
//
// Two versioned specs are exposed:
//   kInputSpecV1 — uses RpcInvalidHotwallet for type mismatches on 'hotwallet'
//   kInputSpecV2 — uses RpcInvalidParams for type mismatches on 'hotwallet'

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/gateway_balances/Types.hpp>

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/PublicKey.h>
#include <xrpl/protocol/tokens.h>

#include <cstddef>
#include <expected>
#include <set>
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

struct HotWalletConverter {
    static constexpr std::string_view kName = "hotWallet";
    using ValueType = std::set<xrpl::AccountID>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        ValueType result;
        auto const parseOne = [&](FA const& elem) -> bool {
            if (!elem.isString())
                return false;
            auto id = detail::accountFromStringStrict(std::string{elem.asString()});
            if (!id)
                return false;
            result.insert(*id);
            return true;
        };

        if (f.isString()) {
            if (!parseOne(f))
                return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
        } else {
            for (std::size_t i = 0; i < f.arraySize(); ++i) {
                if (!parseOne(f.element(i)))
                    return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
            }
        }
        return result;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto hotWalletConv = HotWalletConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpecV1 = spec<Input>(
    field("account", &Input::account, required, accountId),
    field("ledger_hash", &Input::ledgerHash, ledgerHashHex),
    field("ledger_index", &Input::ledgerIndex, ledgerIndexOpt),
    field("hotwallet", &Input::hotWallets, kHOT_WALLET_V1, hotWalletConv)
);

inline constexpr auto kInputSpecV2 = extend(
    kInputSpecV1,
    field("hotwallet", &Input::hotWallets, kHOT_WALLET_V2, hotWalletConv)
);

} // namespace rpc::spec::handlers::gateway_balances
