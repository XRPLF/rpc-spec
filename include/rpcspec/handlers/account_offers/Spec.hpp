/** @file */
#pragma once
// Shared constexpr spec for the 'account_offers' RPC command.
// Single source of truth — both Clio and xrpld include this file.

#include <xrpl/basics/base_uint.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/account_offers/Types.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace rpc::spec::handlers::account_offers {

struct AccountMarkerStrConverter
{
    static constexpr std::string_view kName = "accountMarker";
    using ValueType = std::string;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotString"}};
        }
        auto const sv = f.asString();
        auto const malformed = [&] {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams,
                "Invalid field '" + std::string{f.key()} + "', not hex string."}};
        };
        auto const commaPos = sv.find(',');
        if (commaPos == std::string_view::npos)
            return malformed();
        auto const hexPart = std::string{sv.substr(0, commaPos)};
        auto const hintPart = sv.substr(commaPos + 1);
        xrpl::uint256 index;
        if (!index.parseHex(hexPart.c_str()))
            return malformed();
        uint64_t hint = 0;
        auto const [ptr, ec] =
            std::from_chars(hintPart.data(), hintPart.data() + hintPart.size(), hint);
        if (ec != std::errc() || ptr != hintPart.data() + hintPart.size())
            return malformed();
        return std::string{sv};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr auto accountMarkerStr = AccountMarkerStrConverter{};

inline constexpr auto kInputSpecV1 = spec<Input>(
    ledgerSelector(&Input::ledger, withLegacyLedgerField),
    field("account", &Input::account, required, accountId),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{1}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        defaultTo(kLimitDefault),
        asUint32),
    field("marker", &Input::marker, accountMarkerStr),
    field("strict", deprecated));

inline constexpr auto kInputSpecV2 = kInputSpecV1;

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpecV1, kInputSpecV2);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::account_offers
