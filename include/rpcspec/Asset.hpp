/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Asset.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/AssetTypes.hpp>
#include <rpcspec/Concepts.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/DeferredTypes.hpp>
#include <rpcspec/ServerConditional.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace rpc::spec {

namespace detail {

/**
 * @brief Parse an asset as libxrpl's `assetFromJson` does, or say why it is not one.
 *
 * An object with either a `currency` (and, unless the currency is XRP, a base58 `issuer` that is
 * neither the XRP account nor account one) or a hex `mpt_issuance_id`, never both. An XRP
 * currency may carry a `null` issuer. `badCurrency` and `noCurrency` are refused.
 *
 * @tparam View The field-view type supplied by the backend.
 * @param fieldView The field to read.
 * @return The asset, or @ref DeferredError::Malformed.
 */
template <SomeFieldView View>
[[nodiscard]] DeferredAsset
assetFromView(View const& fieldView)
{
    auto const malformed = std::unexpected{DeferredError::Malformed};
    if (not fieldView.isObject())
        return malformed;

    auto const currencyView = fieldView.child("currency");
    auto const mptView = fieldView.child("mpt_issuance_id");
    auto const issuerView = fieldView.child("issuer");

    if (currencyView.present())
    {
        if (mptView.present() or not currencyView.isString())
            return malformed;

        xrpl::Currency currency;
        if (not xrpl::toCurrency(currency, std::string{currencyView.asString()}) or
            currency == xrpl::badCurrency() or currency == xrpl::noCurrency())
            return malformed;

        if (xrpl::isXRP(currency))
        {
            if (issuerView.present() and not issuerView.isNull())
                return malformed;
            return xrpl::xrpIssue();
        }

        if (not issuerView.isString())
            return malformed;
        auto const issuer = xrpl::parseBase58<xrpl::AccountID>(std::string{issuerView.asString()});
        if (not issuer.has_value() or *issuer == xrpl::noAccount() or *issuer == xrpl::xrpAccount())
            return malformed;
        return xrpl::Issue{currency, *issuer};
    }

    if (not mptView.present() or issuerView.present() or not mptView.isString())
        return malformed;

    xrpl::MPTID id;
    if (not id.parseHex(std::string{mptView.asString()}))
        return malformed;
    return xrpl::MPTIssue{id};
}

}  // namespace detail

/**
 * @brief Converts an asset field into a @ref DeferredAsset.
 *
 * On xrpld every value is accepted, and one that does not describe an asset is carried as
 * @ref DeferredError::Malformed for the handler to report. On Clio @p ClioConv parses the field,
 * and its failure fails the parse.
 *
 * @tparam ClioConv The converter Clio parses the field with; its value must convert to an asset.
 */
template <typename ClioConv>
struct DeferredAssetConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("asset").
     */
    static constexpr std::string_view kName = "asset";

    /**
     * @brief The value this converter produces (`DeferredAsset`).
     */
    using ValueType = DeferredAsset;

    /**
     * @brief The converter Clio parses the field with.
     */
    ClioConv clioConv;

    /**
     * @brief Validate the field and produce its value.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to read.
     * @return The asset, a deferred parsing failure, or a Status describing the failure.
     */
    template <SomeFieldView View>
    [[nodiscard]] Parsed<ValueType>
    parse(View const& fieldView) const
    {
        if constexpr (kIsXrpldBuild)
        {
            return detail::assetFromView(fieldView);
        }
        else
        {
            auto value = clioConv.parse(fieldView);
            if (not value.has_value())
                return std::unexpected{std::move(value).error()};
            return ValueType{xrpl::Asset{std::move(value).value()}};
        }
    }
};

/**
 * @brief An asset whose parsing failure the xrpld handler reports; Clio parses it with @p conv.
 *
 * @param conv The converter Clio parses the field with.
 * @return The deferring converter.
 */
template <typename ClioConv>
[[nodiscard]] consteval auto
deferredAsset(ClioConv conv)
{
    return DeferredAssetConverter<ClioConv>{conv};
}

}  // namespace rpc::spec
