/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/vault_info/Types.hpp>

#include <cstdint>
#include <expected>
#include <string_view>

namespace rpc::spec::handlers::vault_info {

// Clio reports every malformed vault_info field as ClioError::RpcMalformedRequest - bare for
// vault_id and seq, and carrying "OwnerNotHexString" for owner. xrpld's parseVault() uses
// field-specific rippled codes instead.
#if defined(RPCSPEC_IS_CLIO)
inline constexpr auto kVaultFieldError = rpc::kMalformedRequest;
inline constexpr std::string_view kVaultIdMessage = {};
inline constexpr auto kOwnerError = rpc::kMalformedRequest;
inline constexpr std::string_view kOwnerMessage = "OwnerNotHexString";
inline constexpr std::string_view kSeqMessage = {};
#else
inline constexpr auto kVaultFieldError = rpc::CombinedError{rpc::RippledError::RpcInvalidParams};
inline constexpr std::string_view kVaultIdMessage = "Invalid field 'vault_id', not hex string.";
inline constexpr auto kOwnerError = rpc::CombinedError{rpc::RippledError::RpcActMalformed};
inline constexpr std::string_view kOwnerMessage = "Invalid field 'owner', not AccountID.";
inline constexpr std::string_view kSeqMessage =
    "Invalid field 'seq', not a positive 32-bit integer.";
#endif

/**
 * @brief Resolves `owner` to an AccountID, reporting the per-server error above on failure.
 *
 * Paired with the `accountBase58` validator on the same field, which rejects the same inputs -
 * this converter's error path is a backstop, so it must report the identical status rather than
 * a converter-specific one.
 */
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
        if (kOwnerMessage.empty())
            return std::unexpected{rpc::Status{kOwnerError}};
        return std::unexpected{rpc::Status{kOwnerError, std::string{kOwnerMessage}}};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto ownerConv = OwnerConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    // uint256Hex and asUint256 accept exactly the same inputs, so the converter never reports
    // on its own; the withCustomError on the validator is what clients see.
    field(
        "vault_id",
        &Input::vaultID,
        withCustomError(uint256Hex, kVaultFieldError, kVaultIdMessage),
        asUint256),
    field(
        "owner",
        &Input::owner,
        // accountBase58 additionally rejects the zero AccountID, which xrpld's parseVault()
        // accepts, so it is applied on the Clio side only.
        ifServerClio(withCustomError(accountBase58, kOwnerError, kOwnerMessage)),
        ownerConv),
    field(
        "seq",
        &Input::tnxSequence,
        withCustomError(type<uint32_t>, kVaultFieldError, kSeqMessage),
        asUint32));

/**
 * @brief Version-selecting spec (resolved from Input via specFor).
 */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/**
 * @brief ADL hook: resolve the versioned spec from the Input type.
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::vault_info
