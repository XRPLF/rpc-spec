/** @file */
#pragma once
// Shared constexpr spec for the 'nfts_by_issuer' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/handlers/nfts_by_issuer/Types.hpp>

#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <string_view>

namespace rpc::spec::handlers::nfts_by_issuer {

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
    field("issuer", &Input::issuer, required, accountId),
    field("nft_taxon", &Input::nftTaxon, type<uint32_t>, asUint32),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        asUint32
    ),
    field("marker", &Input::marker, asUint256)
);

inline constexpr auto& kSpec = kInputSpec;

} // namespace rpc::spec::handlers::nfts_by_issuer
