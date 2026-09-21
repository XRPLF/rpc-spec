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

template <rpc::RippledError IsrErr>
struct TakerAssetConverter
{
    static constexpr std::string_view kName = "takerAsset";
    using ValueType = xrpl::Asset;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const mptFa = f.child("mpt_issuance_id");
        if (mptFa.present())
        {
            xrpl::MPTID mptId{};
            if (!mptId.parseHex(std::string{mptFa.asString()}))
                return std::unexpected{rpc::Status{IsrErr}};  // unreachable: section validated hex
            return xrpl::MPTIssue{mptId};
        }

        auto const currSv = f.child("currency").asString();
        xrpl::Currency currency{};
        if (!xrpl::toCurrency(currency, std::string{currSv}))
            return std::unexpected{rpc::Status{IsrErr}};  // unreachable: section validated currency
        bool const hasIssuer = f.child("issuer").present();
        if (xrpl::isXRP(currency))
        {
            // XRP must not be paired with an issuer.
            if (hasIssuer)
            {
                return std::unexpected{rpc::Status{
                    IsrErr,
                    "Unneeded field '" + std::string{f.key()} +
                        ".issuer' for XRP currency specification."}};
            }
            return xrpl::xrpIssue();
        }
        if (!hasIssuer)
            return xrpl::Issue{currency, xrpl::xrpAccount()};
        auto const issuerSv = f.child("issuer").asString();
        xrpl::AccountID issuer{};
        if (!xrpl::toIssuer(issuer, std::string{issuerSv}))
            return std::unexpected{rpc::Status{IsrErr}};
        return xrpl::Issue{currency, issuer};
    }
};

struct TakerConverter
{
    static constexpr std::string_view kName = "taker";
    using ValueType = xrpl::AccountID;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const err = [] {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "Invalid field 'taker'."}};
        };
        if (!f.isString())
            return err();
        auto id = detail::accountFromStringStrict(std::string{f.asString()});
        if (!id)
            return err();
        return *id;
    }
};

inline constexpr auto kTakerValidator = CustomValidator{[](auto const& f) -> MaybeError {
    if (!f.isObject())
        return {};

    auto const currencyFa = f.child("currency");
    auto const mptFa = f.child("mpt_issuance_id");
    bool const hasCurrency = currencyFa.present();
    bool const hasMptId = mptFa.present();

    if (!hasCurrency && !hasMptId)
    {
#if defined(RPCSPEC_IS_CLIO)
        // Clio requires `currency` inside the taker section, so a request naming neither
        // fails with that Required message rather than one mentioning mpt_issuance_id.
        return std::unexpected{
            rpc::Status{rpc::RippledError::RpcInvalidParams, "Required field 'currency' missing"}};
#else
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Missing field '" + std::string{f.key()} + ".currency'."}};
#endif
    }

    if (hasMptId && (hasCurrency || f.child("issuer").present()))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams, "Invalid field '" + std::string{f.key()} + "'."}};
    }

#if !defined(RPCSPEC_IS_CLIO)
    // Clio deliberately omits this check and leaves a non-string value to the section's own
    // withCustomError(currency|uint192Hex, Rpc{Src,Dst}...Malformed); checking it here would
    // preempt that and downgrade the code to invalidParams.
    if ((hasCurrency && !currencyFa.isString()) || (hasMptId && !mptFa.isString()))
    {
        return std::unexpected{rpc::Status{
            rpc::RippledError::RpcInvalidParams,
            "Invalid field '" + std::string{f.key()} + ".currency', not string."}};
    }
#endif

    return {};
}};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto takerPaysConv = TakerAssetConverter<RippledError::RpcSrcIsrMalformed>{};
inline constexpr auto takerGetsConv = TakerAssetConverter<RippledError::RpcDstIsrMalformed>{};
inline constexpr auto takerConv = TakerConverter{};
// NOLINTEND(readability-identifier-naming)

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
 */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::book_offers
