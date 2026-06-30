/** @file */
#pragma once
// Shared constexpr spec for the 'nft_info' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/nft_info/Types.hpp>

#include <xrpl/basics/base_uint.h>

#include <string_view>

namespace rpc::spec::handlers::nft_info {

struct Uint256Converter {
    static constexpr std::string_view kName = "uint256";
    using ValueType = xrpl::uint256;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."
            }};
        };
        if (!f.isString())
            return err();
        xrpl::uint256 out;
        if (!out.parseHex(std::string{f.asString()}.c_str()))
            return err();
        return out;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto asUint256 = Uint256Converter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("nft_id", &Input::nftID, required, asUint256)
);

inline constexpr auto& kSpec = kInputSpec;

}  // namespace rpc::spec::handlers::nft_info
