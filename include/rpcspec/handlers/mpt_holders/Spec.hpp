/** @file */
#pragma once
// Shared constexpr spec for the 'mpt_holders' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/mpt_holders/Types.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::mpt_holders {

struct Uint192Converter
{
    static constexpr std::string_view kName = "uint192Hex";
    using ValueType = xrpl::uint192;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        if (!f.isString())
            return err();
        xrpl::uint192 out;
        if (!out.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return out;
    }
};

struct AccountIdHexConverter
{
    static constexpr std::string_view kName = "uint160Hex";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        if (!f.isString())
            return err();
        xrpl::AccountID out;
        if (!out.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return out;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto uint192Conv = Uint192Converter{};
inline constexpr auto accountIdHex = AccountIdHexConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("mpt_issuance_id", &Input::mptID, required, uint192Conv),
    field("marker", &Input::marker, accountIdHex),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32));

}  // namespace rpc::spec::handlers::mpt_holders
