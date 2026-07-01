/** @file */
#pragma once
// Shared constexpr spec for the 'unsubscribe' RPC command.
// Single source of truth — both Clio and rippled include this file.

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/unsubscribe/Types.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace rpc::spec::handlers::unsubscribe {

// Validates an array of account identifiers (base58 or hex pubkey).
// Errors mirror subscribeAccountsValidator from the old system exactly:
//   - not array → RpcInvalidParams + key + "NotArray"
//   - empty array → RpcActMalformed + key + " malformed."
//   - element not string → RpcInvalidParams + key + "'sItemNotString"
//   - element invalid account → RpcActMalformed + key + "'sItemMalformed"
static constexpr auto kSubscribeAccountsValidator =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isArray())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotArray"}};
        }
        if (f.arraySize() == 0)
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcActMalformed, std::string{f.key()} + " malformed."}};
        }
        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            auto const elem = f.element(i);
            if (!elem.isString())
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "'sItemNotString"}};
            }
            if (!rpc::spec::detail::accountFromStringStrict(std::string{elem.asString()}))
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcActMalformed, std::string{f.key()} + "'sItemMalformed"}};
            }
        }
        return {};
    }};

// Validates the streams field: must be an array of known stream name strings.
// The accepted set is server-conditional (the spec is shared):
//   - both servers serve the six common streams below;
//   - rippled additionally accepts `server`/`peer_status`/`consensus` (the admin
//     role for the first two is enforced later, not here) and the deprecated
//     `rt_transactions` alias of `transactions_proposed`;
//   - Clio does not serve those, so it rejects them with RpcNotSupported.
// Errors otherwise mirror the old subscribeStreamValidator: not array →
// "<key>NotArray"; element not string → "streamNotString"; unknown → RpcStreamMalformed.
// A dedicated validator (rather than a CustomValidator lambda) so the accepted
// stream values are exposed to the schema dump via describeParams().
struct StreamsValidator
{
    static constexpr std::string_view kName = "streams";

    // Streams both servers serve.
    static constexpr std::array<std::string_view, 6> kCommon{
        "ledger",
        "transactions",
        "transactions_proposed",
        "book_changes",
        "manifests",
        "validations"};
#if RPCSPEC_IS_CLIO
    // Clio does not serve these live streams.
    static constexpr std::array<std::string_view, 3> kNotSupported{
        "server",
        "peer_status",
        "consensus"};
#else
    // rippled also accepts these (admin role enforced later); rt_transactions is a
    // deprecated alias for transactions_proposed.
    static constexpr std::array<std::string_view, 4> kRippledExtra{
        "server",
        "peer_status",
        "consensus",
        "rt_transactions"};
#endif

    static constexpr bool
    contains(auto const& arr, std::string_view s)
    {
        for (auto const& v : arr)
        {
            if (v == s)
                return true;
        }
        return false;
    }

    template <typename Writer>
    void
    describeParams(Writer& w) const
    {
        w.paramList("oneOf", kCommon);
#if RPCSPEC_IS_CLIO
        w.paramList("notSupported", kNotSupported);
#else
        w.paramList("alsoAllowed", kRippledExtra);
#endif
    }

    template <SomeFieldView FA>
    [[nodiscard]] MaybeError
    verify(FA const& f) const
    {
        if (!f.present())
            return {};
        if (!f.isArray())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotArray"}};
        }
        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            auto const elem = f.element(i);
            if (!elem.isString())
            {
                return std::unexpected{
                    rpc::Status{rpc::RippledError::RpcInvalidParams, "streamNotString"}};
            }
            auto const str = elem.asString();
#if RPCSPEC_IS_CLIO
            if (contains(kNotSupported, str))
                return std::unexpected{rpc::Status{rpc::RippledError::RpcNotSupported}};
            if (!contains(kCommon, str))
                return std::unexpected{rpc::Status{rpc::RippledError::RpcStreamMalformed}};
#else
            if (!contains(kCommon, str) && !contains(kRippledExtra, str))
                return std::unexpected{rpc::Status{rpc::RippledError::RpcStreamMalformed}};
#endif
        }
        return {};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
inline constexpr auto kSUBSCRIBE_STREAM_VALIDATOR = StreamsValidator{};

// Validates the books field: must be an array of valid book objects.
// Errors mirror the old kBOOKS_VALIDATOR lambda exactly (including all parseBook errors).
// Note: Unsubscribe does NOT check snapshot (no snapshot field in unsubscribe).
static constexpr auto kBooksValidator = CustomValidator{[](auto const& f) -> MaybeError {
    if (!f.isArray())
    {
        return std::unexpected{
            rpc::Status{rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "NotArray"}};
    }
    for (std::size_t i = 0; i < f.arraySize(); ++i)
    {
        auto const book = f.element(i);
        if (!book.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, std::string{f.key()} + "ItemNotObject"}};
        }

        auto const bothFa = book.child("both");
        if (bothFa.present() && !bothFa.isBool())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams, "bothNotBool"}};
        }

        auto const takerPaysFa = book.child("taker_pays");
        if (!takerPaysFa.present())
        {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "Missing field 'taker_pays'"}};
        }
        if (!takerPaysFa.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, "Field 'taker_pays' is not an object"}};
        }

        auto const takerGetsFa = book.child("taker_gets");
        if (!takerGetsFa.present())
        {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "Missing field 'taker_gets'"}};
        }
        if (!takerGetsFa.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcInvalidParams, "Field 'taker_gets' is not an object"}};
        }

        auto const paysCurFa = takerPaysFa.child("currency");
        if (!paysCurFa.present() || !paysCurFa.isString())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcSrcCurMalformed}};
        }
        xrpl::Currency payCurrency;
        if (!xrpl::toCurrency(payCurrency, std::string{paysCurFa.asString()}))
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcSrcCurMalformed}};
        }

        auto const getsCurFa = takerGetsFa.child("currency");
        if (!getsCurFa.present() || !getsCurFa.isString())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcDstAmtMalformed}};
        }
        xrpl::Currency getCurrency;
        if (!xrpl::toCurrency(getCurrency, std::string{getsCurFa.asString()}))
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcDstAmtMalformed}};
        }

        // book-level domain (mirrors parseBook): must be string if present
        auto const domainFa = book.child("domain");
        if (domainFa.present() && !domainFa.isString())
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcDomainMalformed}};
        }

        xrpl::AccountID payIssuer;
        auto const paysIssuerFa = takerPaysFa.child("issuer");
        if (paysIssuerFa.present())
        {
            if (!paysIssuerFa.isString())
            {
                return std::unexpected{
                    rpc::Status{rpc::RippledError::RpcInvalidParams, "takerPaysIssuerNotString"}};
            }
            if (!xrpl::toIssuer(payIssuer, std::string{paysIssuerFa.asString()}))
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcSrcIsrMalformed}};
            }
            if (payIssuer == xrpl::noAccount())
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcSrcIsrMalformed}};
            }
        }
        else
        {
            payIssuer = xrpl::xrpAccount();
        }

        if (xrpl::isXRP(payCurrency) && !xrpl::isXRP(payIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcSrcIsrMalformed,
                "Unneeded field 'taker_pays.issuer' for XRP currency specification."}};
        }
        if (!xrpl::isXRP(payCurrency) && xrpl::isXRP(payIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcSrcIsrMalformed,
                "Invalid field 'taker_pays.issuer', expected non-XRP issuer."}};
        }

        xrpl::AccountID getIssuer;
        auto const getsIssuerFa = takerGetsFa.child("issuer");
        if (getsIssuerFa.present())
        {
            if (!getsIssuerFa.isString())
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcInvalidParams, "taker_gets.issuer should be string"}};
            }
            if (!xrpl::toIssuer(getIssuer, std::string{getsIssuerFa.asString()}))
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcDstIsrMalformed,
                    "Invalid field 'taker_gets.issuer', bad issuer."}};
            }
            if (getIssuer == xrpl::noAccount())
            {
                return std::unexpected{rpc::Status{
                    rpc::RippledError::RpcDstIsrMalformed,
                    "Invalid field 'taker_gets.issuer', bad issuer account one."}};
            }
        }
        else
        {
            getIssuer = xrpl::xrpAccount();
        }

        if (xrpl::isXRP(getCurrency) && !xrpl::isXRP(getIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcDstIsrMalformed,
                "Unneeded field 'taker_gets.issuer' for XRP currency specification."}};
        }
        if (!xrpl::isXRP(getCurrency) && xrpl::isXRP(getIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::RippledError::RpcDstIsrMalformed,
                "Invalid field 'taker_gets.issuer', expected non-XRP issuer."}};
        }

        if (payCurrency == getCurrency && payIssuer == getIssuer)
        {
            return std::unexpected{rpc::Status{rpc::RippledError::RpcBadMarket, "badMarket"}};
        }

        // book-level domain (mirrors inner parseBook overload): must parse as hex
        if (domainFa.present())
        {
            xrpl::uint256 dom;
            if (!dom.parseHex(std::string{domainFa.asString()}))
            {
                return std::unexpected{rpc::Status{rpc::RippledError::RpcDomainMalformed}};
            }
        }
    }
    return {};
}};

struct StreamVecConverter
{
    static constexpr std::string_view kName = "streamVec";
    using ValueType = std::optional<std::vector<StreamType>>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        // The streams validator already rejected unknown/unsupported names, so each
        // element here is an accepted stream for this server build.
        std::vector<StreamType> result;
        result.reserve(f.arraySize());
        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            auto const s = f.element(i).asString();
            if (s == "ledger")
            {
                result.push_back(StreamType::Ledger);
            }
            else if (s == "transactions")
            {
                result.push_back(StreamType::Transactions);
            }
            else if (s == "transactions_proposed" || s == "rt_transactions")
            {  // rt_transactions: deprecated alias
                result.push_back(StreamType::TransactionsProposed);
            }
            else if (s == "book_changes")
            {
                result.push_back(StreamType::BookChanges);
            }
            else if (s == "manifests")
            {
                result.push_back(StreamType::Manifests);
            }
            else if (s == "validations")
            {
                result.push_back(StreamType::Validations);
            }
            else if (s == "server")
            {
                result.push_back(StreamType::Server);
            }
            else if (s == "peer_status")
            {
                result.push_back(StreamType::PeerStatus);
            }
            else
            {  // "consensus"
                result.push_back(StreamType::Consensus);
            }
        }
        return std::optional<std::vector<StreamType>>{std::move(result)};
    }
};

struct AccountIdVecConverter
{
    static constexpr std::string_view kName = "accountIdVec";
    using ValueType = std::optional<std::vector<xrpl::AccountID>>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        // The accounts validator already confirmed each element is a valid account.
        std::vector<xrpl::AccountID> result;
        result.reserve(f.arraySize());
        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            result.push_back(
                rpc::spec::detail::accountFromValidated(std::string{f.element(i).asString()}));
        }
        return std::optional<std::vector<xrpl::AccountID>>{std::move(result)};
    }
};

struct UnsubscribeBooksConverter
{
    static constexpr std::string_view kName = "unsubscribeBooksVec";
    using ValueType = std::optional<std::vector<OrderBook>>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        std::vector<OrderBook> result;
        result.reserve(f.arraySize());

        for (std::size_t i = 0; i < f.arraySize(); ++i)
        {
            auto const bookFa = f.element(i);
            OrderBook ob;

            auto const bothFa = bookFa.child("both");
            if (bothFa.present())
                ob.both = bothFa.asBool();

            auto const paysFa = bookFa.child("taker_pays");
            auto const getsFa = bookFa.child("taker_gets");

            auto const payCurrency = rpc::spec::detail::currencyFromValidated(
                std::string{paysFa.child("currency").asString()});
            auto const getCurrency = rpc::spec::detail::currencyFromValidated(
                std::string{getsFa.child("currency").asString()});

            auto const paysIssuerFa = paysFa.child("issuer");
            xrpl::AccountID const payIssuer = paysIssuerFa.present()
                ? rpc::spec::detail::issuerFromValidated(std::string{paysIssuerFa.asString()})
                : xrpl::xrpAccount();

            auto const getsIssuerFa = getsFa.child("issuer");
            xrpl::AccountID const getIssuer = getsIssuerFa.present()
                ? rpc::spec::detail::issuerFromValidated(std::string{getsIssuerFa.asString()})
                : xrpl::xrpAccount();

            std::optional<xrpl::uint256> domainID;
            auto const domainFa = bookFa.child("domain");
            if (domainFa.present())
            {
                domainID =
                    rpc::spec::detail::uint256FromValidated(std::string{domainFa.asString()});
            }

            ob.book = xrpl::Book{
                xrpl::Issue{payCurrency, payIssuer}, xrpl::Issue{getCurrency, getIssuer}, domainID};

            result.push_back(ob);
        }

        return std::optional<std::vector<OrderBook>>{std::move(result)};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto streamVecConv = StreamVecConverter{};
inline constexpr auto accountIdVecConv = AccountIdVecConverter{};
inline constexpr auto unsubscribeBooksConv = UnsubscribeBooksConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    field("streams", &Input::streams, kSUBSCRIBE_STREAM_VALIDATOR, streamVecConv),
    field("accounts", &Input::accounts, kSubscribeAccountsValidator, accountIdVecConv),
    field(
        "accounts_proposed",
        &Input::accountsProposed,
        kSubscribeAccountsValidator,
        accountIdVecConv),
    field("books", &Input::books, kBooksValidator, unsubscribeBooksConv),
    field("url") | deprecated,
    field("rt_accounts") | deprecated,
    field("rt_transactions") | deprecated);

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::unsubscribe
