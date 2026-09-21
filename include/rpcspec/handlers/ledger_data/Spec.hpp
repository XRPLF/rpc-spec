/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/LedgerTypes.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/ledger_data/Types.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace rpc::spec::handlers::ledger_data {

struct MarkerConverter
{
    static constexpr std::string_view kName = "marker";
    using ValueType = std::optional<MarkerValue>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
        {
            xrpl::uint256 parsed;
            auto const sv = f.asString();
            if (!parsed.parseHex(std::string{sv}.c_str()))
            {
                return std::unexpected{
                    rpc::Status{rpc::kMalformedField, rpc::malformedFieldMessage("marker")}};
            }
            return std::optional<MarkerValue>{MarkerValue{parsed}};
        }
        if (f.isUint32())
            return std::optional<MarkerValue>{MarkerValue{f.asUint32()}};
#if defined(RPCSPEC_IS_CLIO)
        // Anything else (bool, object, negative int, ...) is a plain type failure and carries
        // no message.
        return std::unexpected{rpc::Status{rpc::kMalformedField}};
#else
        return std::unexpected{rpc::Status{rpc::kMalformedField, "markerNotString"}};
#endif
    }
};

struct LedgerEntryTypeConverter
{
    static constexpr std::string_view kName = "ledgerEntryType";
    using ValueType = xrpl::LedgerEntryType;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (!f.isString())
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::expectedFieldMessage(f.key(), "string")}};
        }
        auto const entryType = ledgerEntryTypeFromStr(std::string{f.asString()});
        if (entryType == xrpl::ltANY)
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::invalidFieldMessage(f.key())}};
        }
        return entryType;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto markerConv = MarkerConverter{};
inline constexpr auto ledgerEntryTypeConv = LedgerEntryTypeConverter{};
// NOLINTEND(readability-identifier-naming)

// marker and diffMarker are unified into a single optional<MarkerValue> member;
// cross-field validation (outOfOrder + marker type) stays in process().
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("binary", &Input::binary, jsonBoolStrict),
    // No defaultTo: the effective default is kLimitBinary or kLimitJson depending on
    // `binary`, so the handler resolves it (see Input::limit).
    field("limit", &Input::limit, type<uint32_t>, min(uint32_t{1}), asUint32),
    field("marker", &Input::marker, markerConv),
    field("out_of_order", &Input::outOfOrder, jsonBoolStrict),
    field("type", &Input::type, ledgerEntryTypeConv),
    field("ledger", deprecated)  // validate-only: emits a deprecation warning, not stored
);

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

}  // namespace rpc::spec::handlers::ledger_data
