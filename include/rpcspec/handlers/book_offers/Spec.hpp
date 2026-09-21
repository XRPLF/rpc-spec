/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Asset.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/handlers/book_offers/Types.hpp>

#include <expected>
#include <string>

namespace rpc::spec::handlers::book_offers {

/**
 * @brief Converts the taker asset field into its strongly-typed value.
 */
template <rpc::RippledError IsrErr>
struct TakerAssetConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("takerAsset").
     */
    static constexpr std::string_view kName = "takerAsset";

    /**
     * @brief The value this converter produces (`xrpl::Asset`).
     */
    using ValueType = xrpl::Asset;

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
        auto const mptView = fieldView.child("mpt_issuance_id");
        if (mptView.present())
        {
            xrpl::MPTID mptId{};
            if (not mptId.parseHex(std::string{mptView.asString()}))
                return std::unexpected{rpc::Status{IsrErr}};  // unreachable: section validated hex
            return xrpl::MPTIssue{mptId};
        }

        auto const currSv = fieldView.child("currency").asString();
        xrpl::Currency currency{};
        if (not xrpl::toCurrency(currency, std::string{currSv}))
            return std::unexpected{rpc::Status{IsrErr}};  // unreachable: section validated currency
        bool const hasIssuer = fieldView.child("issuer").present();
        if (xrpl::isXRP(currency))
        {
            // XRP must not be paired with an issuer.
            if (hasIssuer)
            {
                return std::unexpected{rpc::Status{
                    IsrErr,
                    "Unneeded field '" + std::string{fieldView.key()} +
                        ".issuer' for XRP currency specification."}};
            }
            return xrpl::xrpIssue();
        }
        if (not hasIssuer)
            return xrpl::Issue{currency, xrpl::xrpAccount()};
        auto const issuerSv = fieldView.child("issuer").asString();
        xrpl::AccountID issuer{};
        if (not xrpl::toIssuer(issuer, std::string{issuerSv}))
            return std::unexpected{rpc::Status{IsrErr}};
        return xrpl::Issue{currency, issuer};
    }
};

/**
 * @brief Converts the taker field into its strongly-typed value.
 */
struct TakerConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("taker").
     */
    static constexpr std::string_view kName = "taker";

    /**
     * @brief The value this converter produces (`xrpl::AccountID`).
     */
    using ValueType = xrpl::AccountID;

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
        auto const err = [] {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "Invalid field 'taker'."}};
        };
        if (not fieldView.isString())
            return err();
        auto id = detail::accountFromStringStrict(std::string{fieldView.asString()});
        if (not id.has_value())
            return err();
        return *id;
    }
};

/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kTakerValidator = CustomValidator{[](auto const& fieldView) -> MaybeError {
    if (not fieldView.isObject())
        return {};

    auto const currencyView = fieldView.child("currency");
    auto const mptView = fieldView.child("mpt_issuance_id");
    bool const hasCurrency = currencyView.present();
    bool const hasMptId = mptView.present();

    if (not hasCurrency and not hasMptId)
    {
#if defined(RPCSPEC_IS_CLIO)
        // Clio requires `currency` inside the taker section, so a request naming neither
        // fails with that Required message rather than one mentioning mpt_issuance_id.
        return std::unexpected{
            rpc::Status{rpc::RippledError::RpcInvalidParams, "Required field 'currency' missing"}};
#else
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Missing field '" + std::string{fieldView.key()} + ".currency'."}};
#endif
    }

    if (hasMptId and (hasCurrency or fieldView.child("issuer").present()))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field '" + std::string{fieldView.key()} + "'."}};
    }

#if !defined(RPCSPEC_IS_CLIO)
    // Clio deliberately omits this check and leaves a non-string value to the section's own
    // withCustomError(currency|uint192Hex, Rpc{Src,Dst}...Malformed); checking it here would
    // preempt that and downgrade the code to invalidParams.
    if ((hasCurrency and not currencyView.isString()) or (hasMptId and not mptView.isString()))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field '" + std::string{fieldView.key()} + ".currency', not string."}};
    }
#endif

    return {};
}};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: taker asset.
 */
inline constexpr auto takerPaysConv = TakerAssetConverter<RippledError::RpcSrcIsrMalformed>{};

/**
 * @brief Converter instance: taker asset.
 */
inline constexpr auto takerGetsConv = TakerAssetConverter<RippledError::RpcDstIsrMalformed>{};

/**
 * @brief Converter instance: taker.
 */
inline constexpr auto takerConv = TakerConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    field(
        "taker_gets",
        &Input::takerGets,
        required,
        type<JsonObject>,
        kTakerValidator,
        section(
            field("currency", withCustomError(currency, RippledError::RpcDstAmtMalformed)),
            field("mpt_issuance_id", withCustomError(uint192Hex, RippledError::RpcDstAmtMalformed)),
            field("issuer", withCustomError(issuer, RippledError::RpcDstIsrMalformed))),
        takerGetsConv),
    field(
        "taker_pays",
        &Input::takerPays,
        required,
        type<JsonObject>,
        kTakerValidator,
        section(
            field("currency", withCustomError(currency, RippledError::RpcSrcCurMalformed)),
            field("mpt_issuance_id", withCustomError(uint192Hex, RippledError::RpcSrcCurMalformed)),
            field("issuer", withCustomError(issuer, RippledError::RpcSrcIsrMalformed))),
        takerPaysConv),
    field(
        "taker",
        &Input::taker,
        withCustomError(account, RippledError::RpcInvalidParams, "Invalid field 'taker'."),
        takerConv),
    field(
        "domain",
        &Input::domain,
        withCustomError(
            type<std::string>,
            RippledError::RpcDomainMalformed,
            "Unable to parse domain."),
        withCustomError(uint256Hex, RippledError::RpcDomainMalformed, "Unable to parse domain."),
        asString),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        defaultTo(kLimitDefault),
        asUint32),
    ledgerSelector(&Input::ledger));

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

}  // namespace rpc::spec::handlers::book_offers
