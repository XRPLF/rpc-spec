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

/**
 * @brief Converts the marker field into its strongly-typed value.
 */
struct MarkerConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("marker").
     */
    static constexpr std::string_view kName = "marker";

    /**
     * @brief The value this converter produces (`std::optional<MarkerValue>`).
     */
    using ValueType = std::optional<MarkerValue>;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if (fieldView.isString())
        {
            xrpl::uint256 parsed;
            auto const sv = fieldView.asString();
            if (not parsed.parseHex(std::string{sv}.c_str()))
            {
                return std::unexpected{
                    rpc::Status{rpc::kMalformedField, rpc::malformedFieldMessage("marker")}};
            }
            return std::optional<MarkerValue>{MarkerValue{parsed}};
        }
        if (fieldView.isUint32())
            return std::optional<MarkerValue>{MarkerValue{fieldView.asUint32()}};
#if defined(RPCSPEC_IS_CLIO)
        // Anything else (bool, object, negative int, ...) is a plain type failure and carries
        // no message.
        return std::unexpected{rpc::Status{rpc::kMalformedField}};
#else
        return std::unexpected{rpc::Status{rpc::kMalformedField, "markerNotString"}};
#endif
    }
};

/**
 * @brief Converts the ledger entry type field into its strongly-typed value.
 */
struct LedgerEntryTypeConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("ledgerEntryType").
     */
    static constexpr std::string_view kName = "ledgerEntryType";

    /**
     * @brief The value this converter produces (`xrpl::LedgerEntryType`).
     */
    using ValueType = xrpl::LedgerEntryType;

    /**
     * @brief Validate the field and produce its strongly-typed value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The converted value, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if (not fieldView.isString())
        {
            return std::unexpected{rpc::Status{
                rpc::kMalformedField, rpc::expectedFieldMessage(fieldView.key(), "string")}};
        }
        auto const entryType = ledgerEntryTypeFromStr(std::string{fieldView.asString()});
        if (entryType == xrpl::ltANY)
        {
            return std::unexpected{
                rpc::Status{rpc::kMalformedField, rpc::invalidFieldMessage(fieldView.key())}};
        }
        return entryType;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: marker.
 */
inline constexpr auto markerConv = MarkerConverter{};

/**
 * @brief Converter instance: ledger entry type.
 */
inline constexpr auto ledgerEntryTypeConv = LedgerEntryTypeConverter{};
// NOLINTEND(readability-identifier-naming)

// marker and diffMarker are unified into a single optional<MarkerValue> member;
// cross-field validation (outOfOrder + marker type) stays in process().
/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
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
 *
 * @return A reference to this handler's `kSpec`, for `HandlerFor` to select a version from.
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::ledger_data
