/** @file */
#pragma once

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/ServerConditional.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/get_aggregate_price/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
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

        // Like xrpld, a present but invalid document id is RpcInvalidParams, not
        // RpcOracleMalformed: both the type gate (e.g. -1, null, 2.3) and the
        // string-to-number conversion (e.g. "a") propagate their InvalidParams.
        if (auto err = Type<uint32_t, std::string>::verify(docIdView); not err.has_value())
            return err;

        if (auto err = ToNumberModifier::modify(docIdView); not err.has_value())
            return err;

        if (auto err = AccountBase58Validator::verify(accountView); not err.has_value())
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams}};
    }

    return {};
}};

/**
 * @brief Converts the oracles field into its strongly-typed value.
 *
 * On Clio `kOraclesValidator` has already checked every element. On xrpld only the array itself
 * has been checked: the elements are read as xrpld reads them, and what is wrong with the first
 * bad one is carried in the value for the handler to report once it has resolved the ledger. An
 * element without both keys is @ref DeferredError::Malformed; a document id that is not a uint32
 * (or a string holding one), or an account that is not a base58 string of a non-zero account, is
 * @ref DeferredError::Invalid.
 */
struct OraclesConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("oracles").
     */
    static constexpr std::string_view kName = "oracles";

    /**
     * @brief The value this converter produces (`DeferredOracles`).
     */
    using ValueType = DeferredOracles;

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
        std::vector<Oracle> result;
        result.reserve(fieldView.arraySize());
        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            auto const elem = fieldView.element(i);
            auto const docId = elem.child("oracle_document_id");
            auto const account = elem.child("account");
            if constexpr (kIsXrpldBuild)
            {
                if (not docId.present() or not account.present())
                    return ValueType{std::unexpected{DeferredError::Malformed}};

                auto const documentId = [&] -> std::optional<uint32_t> {
                    if (docId.isUint32())
                        return docId.asUint32();
                    if (docId.isString())
                        return detail::lexicalUint32(docId.asString());
                    return std::nullopt;
                }();
                auto const id = account.isString()
                    ? xrpl::parseBase58<xrpl::AccountID>(std::string{account.asString()})
                    : std::nullopt;
                if (not documentId.has_value() or not id.has_value() or id->isZero())
                    return ValueType{std::unexpected{DeferredError::Invalid}};

                result.push_back(
                    Oracle{
                        .documentId = *documentId,
                        .account = *id,
                    });
            }
            else
            {
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
        }
        return ValueType{std::move(result)};
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
        "oracles",
        &Input::oracles,
        required,
        ifServerXrpld(
            withCustomError(type<JsonArray>, XrpldError::RpcOracleMalformed),
            withCustomError(minArrayLength(1), XrpldError::RpcOracleMalformed),
            withCustomError(maxArrayLength(kOraclesMax), XrpldError::RpcOracleMalformed)),
        ifServerClio(kOraclesValidator),
        oraclesConv),
    field(
        "base_asset",
        &Input::baseAsset,
        required,
        ifServerXrpld(withCustomError(strictCurrency, XrpldError::RpcInvalidParams)),
        ifServerClio(withCustomError(currency, XrpldError::RpcInvalidParams)),
        currencyConv),
    field(
        "quote_asset",
        &Input::quoteAsset,
        required,
        ifServerXrpld(withCustomError(strictCurrency, XrpldError::RpcInvalidParams)),
        ifServerClio(withCustomError(currency, XrpldError::RpcInvalidParams)),
        currencyConv),
    field(
        "trim",
        &Input::trim,
        ifServerXrpld(type<uint32_t, std::string>),
        ifServerClio(type<uint32_t>),
        ifServerXrpld(lexicalUint32),
        between(uint32_t{1}, kTrimMax),
        uint8Conv),
    field(
        "time_threshold",
        &Input::timeThreshold,
        ifServerXrpld(type<uint32_t, std::string>),
        ifServerClio(type<uint32_t>),
        ifServerXrpld(lexicalUint32),
        asUint32));

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
