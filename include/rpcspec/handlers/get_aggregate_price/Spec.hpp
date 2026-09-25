/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace rpc::spec::handlers::get_aggregate_price {

/**
 * @brief Oracles max.
 */
inline constexpr auto kOraclesMax = 200;

/**
 * @brief Validator for the oracles field.
 */
inline constexpr auto kOraclesValidator = CustomModifier{[](auto& fieldView) -> MaybeError {
    if (not fieldView.isArray() or fieldView.arraySize() == 0 or
        fieldView.arraySize() > kOraclesMax)
        return std::unexpected{rpc::Status{rpc::XrpldError::RpcOracleMalformed}};

    for (auto i = 0uz; i < fieldView.arraySize(); ++i)
    {
        auto elem = fieldView.element(i);
        if (not elem.isObject())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcOracleMalformed}};

        auto docIdView = elem.child("oracle_document_id");
        auto accountView = elem.child("account");

        if (not docIdView.present() or not accountView.present())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcOracleMalformed}};

        if (auto err = Type<uint32_t, std::string>::verify(docIdView); not err.has_value())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcOracleMalformed}};

        // Mirrors the old behaviour: RpcInvalidParams when the string is not a valid
        // integer, e.g. "a".
        if (auto err = ToNumberModifier::modify(docIdView); not err.has_value())
            return err;

        if (auto err = AccountBase58Validator::verify(accountView); not err.has_value())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
    }

    return {};
}};

/**
 * @brief Converts the oracles field into its strongly-typed value.
 */
struct OraclesConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("oracles").
     */
    static constexpr std::string_view kName = "oracles";

    /**
     * @brief The value this converter produces (`std::vector<Oracle>`).
     */
    using ValueType = std::vector<Oracle>;

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
        ValueType result;
        result.reserve(fieldView.arraySize());
        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            auto const elem = fieldView.element(i);
            auto const docId = elem.child("oracle_document_id");
            auto const account = elem.child("account");
            // Both are guaranteed valid by kOraclesValidator; extract directly.
            auto id = detail::accountFromStringStrict(std::string{account.asString()});
            if (not id.has_value())
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
            result.push_back(
                Oracle{
                    .documentId = docId.asUint32(),
                    .account = *id,
                });
        }
        return result;
    }
};

/**
 * @brief Converts the uint8 field into its strongly-typed value.
 */
struct Uint8Converter
{
    /**
     * @brief Identifier for this item in the schema dump ("uint8").
     */
    static constexpr std::string_view kName = "uint8";

    /**
     * @brief The value this converter produces (`uint8_t`).
     */
    using ValueType = uint8_t;

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
        if (not fieldView.isUint32())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
        return static_cast<uint8_t>(fieldView.asUint32());
    }
};

/**
 * @brief Converts the currency field into its strongly-typed value.
 */
struct CurrencyConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("currency").
     */
    static constexpr std::string_view kName = "currency";

    /**
     * @brief The value this converter produces (`xrpl::Currency`).
     */
    using ValueType = xrpl::Currency;

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
        // The `currency` validator already confirmed this is a valid currency code string.
        xrpl::Currency currency;
        if (not fieldView.isString() or
            not xrpl::toCurrency(currency, std::string{fieldView.asString()}))
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
        return currency;
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: oracles.
 */
inline constexpr auto oraclesConv = OraclesConverter{};

/**
 * @brief Converter instance: uint8.
 */
inline constexpr auto uint8Conv = Uint8Converter{};

/**
 * @brief Converter instance: currency.
 */
inline constexpr auto currencyConv = CurrencyConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "base_asset",
        &Input::baseAsset,
        required,
        withCustomError(currency, XrpldError::RpcInvalidParams),
        currencyConv),
    field(
        "quote_asset",
        &Input::quoteAsset,
        required,
        withCustomError(currency, XrpldError::RpcInvalidParams),
        currencyConv),
    field("oracles", &Input::oracles, required, kOraclesValidator, oraclesConv),
    field("time_threshold", &Input::timeThreshold, type<uint32_t>, asUint32),
    field("trim", &Input::trim, type<uint32_t>, between(uint32_t{1}, uint32_t{25}), uint8Conv));

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

}  // namespace rpc::spec::handlers::get_aggregate_price
