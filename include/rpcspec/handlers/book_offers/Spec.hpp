/** @file */
#pragma once
// Shared constexpr spec for the 'book_offers' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <xrpl/protocol/AccountID.h>
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
struct CurrencyIssuerConverter
{
    static constexpr std::string_view kName = "currencyIssuer";
    using ValueType = xrpl::Issue;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        auto const currSv = f.child("currency").asString();
        xrpl::Currency currency{};
        if (!xrpl::toCurrency(currency, std::string{currSv}))
            return std::unexpected{rpc::Status{IsrErr}};  // unreachable: section validated currency
        bool const hasIssuer = f.child("issuer").present();
        if (xrpl::isXRP(currency))
        {
            // XRP must not be paired with an issuer.
            if (hasIssuer)
                return std::unexpected{rpc::Status{
                    IsrErr,
                    "Unneeded field '" + std::string{f.key()} +
                        ".issuer' for XRP currency specification."}};
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

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto takerPaysConv = CurrencyIssuerConverter<RippledError::RpcSrcIsrMalformed>{};
inline constexpr auto takerGetsConv = CurrencyIssuerConverter<RippledError::RpcDstIsrMalformed>{};
inline constexpr auto takerConv = TakerConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field(
        "limit",
        &Input::limit,
        type<uint32_t>,
        min(uint32_t{kLimitMin}),
        clamp(uint32_t{kLimitMin}, uint32_t{kLimitMax}),
        defaultTo(kLimitDefault),
        asUint32),
    field(
        "taker",
        &Input::taker,
        withCustomError(account, RippledError::RpcInvalidParams, "Invalid field 'taker'."),
        takerConv),
    field(
        "taker_pays",
        &Input::takerPays,
        required,
        type<JsonObject>,
        section(
            field(
                "currency",
                required,
                withCustomError(currency, RippledError::RpcSrcCurMalformed)),
            field("issuer", withCustomError(issuer, RippledError::RpcSrcIsrMalformed))),
        takerPaysConv),
    field(
        "taker_gets",
        &Input::takerGets,
        required,
        type<JsonObject>,
        section(
            field(
                "currency",
                required,
                withCustomError(currency, RippledError::RpcDstAmtMalformed)),
            field("issuer", withCustomError(issuer, RippledError::RpcDstIsrMalformed))),
        takerGetsConv),
    field(
        "domain",
        &Input::domain,
        withCustomError(
            type<std::string>,
            RippledError::RpcDomainMalformed,
            "Unable to parse domain."),
        withCustomError(uint256Hex, RippledError::RpcDomainMalformed, "Unable to parse domain."),
        asString));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::book_offers
