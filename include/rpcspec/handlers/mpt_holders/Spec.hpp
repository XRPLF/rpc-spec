/** @file */
#pragma once
// Shared constexpr spec for the 'mpt_holders' RPC command.
// Single source of truth — both Clio and xrpld include this file.

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::mpt_holders {

struct AccountIdHexConverter
{
    static constexpr std::string_view kName = "uint160Hex";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::malformedFieldMessage(f.key())}};
        };
        if (!f.isString())
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::notStringFieldMessage(f.key())}};
        }
        xrpl::AccountID out;
        if (!out.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return out;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto accountIdHex = AccountIdHexConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("mpt_issuance_id", &Input::mptID, required, asUint192),
    field("marker", &Input::marker, accountIdHex),
    field("accounts", &Input::accounts, accountIdArray<kMaxAccounts>, asAccountIdVec),
    // No defaultTo: the handler has to tell a client-supplied limit from an absent one,
    // because `accounts` cannot be combined with paging (see Input::accounts).
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::mpt_holders
