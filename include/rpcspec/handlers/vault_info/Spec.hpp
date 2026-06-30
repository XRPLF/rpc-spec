/** @file */
#pragma once
// Shared constexpr spec for the 'vault_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/vault_info/Types.hpp>

#include <cstdint>
#include <expected>
#include <string_view>

namespace rpc::spec::handlers::vault_info {

struct VaultIdConverter {
    static constexpr std::string_view kName = "uint256Hex";
    using ValueType = xrpl::uint256;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString())
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
        xrpl::uint256 out;
        if (!out.parseHex(std::string{f.asString()}.c_str()))
            return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest}};
        return out;
    }
};

struct OwnerConverter {
    static constexpr std::string_view kName = "account";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString()) {
            if (auto id = detail::accountFromStringStrict(std::string{f.asString()}); id)
                return *id;
        }
        return std::unexpected{rpc::Status{rpc::ClioError::RpcMalformedRequest, "OwnerNotHexString"}};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto vaultIdConv = VaultIdConverter{};
inline constexpr auto ownerConv = OwnerConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("vault_id", &Input::vaultID, vaultIdConv),
    field("owner", &Input::owner, ownerConv),
    field("seq", &Input::tnxSequence, withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest), asUint32)
);

} // namespace rpc::spec::handlers::vault_info
