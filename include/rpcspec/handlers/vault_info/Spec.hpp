/** @file */
#pragma once
// Shared constexpr spec for the 'vault_info' RPC command.
// Single source of truth — both Clio and xrpld include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/vault_info/Types.hpp>

#include <cstdint>
#include <expected>
#include <string_view>

namespace rpc::spec::handlers::vault_info {

struct VaultIdConverter
{
    static constexpr std::string_view kName = "uint256Hex";
    using ValueType = xrpl::uint256;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        // Matches xrpld's VaultInfo.cpp parseVault(): a non-string and an unparseable
        // hex string both yield RpcInvalidParams with expectedFieldMessage(vault_id,
        // "hex string").
        auto const err = [] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                rpc::expectedFieldMessage("vault_id", "hex string")}};
        };
        if (!f.isString())
            return err();
        xrpl::uint256 out;
        if (!out.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return out;
    }
};

struct OwnerConverter
{
    static constexpr std::string_view kName = "account";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
        {
            if (auto id = detail::accountFromStringStrict(std::string{f.asString()}); id)
                return *id;
        }
        // Matches xrpld's VaultInfo.cpp parseVault(): RpcActMalformed carrying
        // expectedFieldMessage(owner, "AccountID"), not the generic "Account malformed."
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcActMalformed, rpc::expectedFieldMessage("owner", "AccountID")}};
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
    // xrpld phrases this as expectedFieldMessage(seq, "a positive 32-bit integer");
    // withCustomError is consteval so the message is spelled out rather than built.
    field(
        "seq",
        &Input::tnxSequence,
        withCustomError(
            type<uint32_t>,
            rpc::RippledError::RpcInvalidParams,
            "Invalid field 'seq', not a positive 32-bit integer."),
        asUint32));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::vault_info
