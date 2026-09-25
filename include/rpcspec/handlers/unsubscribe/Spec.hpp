/** @file */
#pragma once

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
/**
 * @brief Validator for the subscribe accounts field.
 */
inline constexpr auto kSubscribeAccountsValidator =
    CustomValidator{[](auto const& fieldView) -> MaybeError {
        if (not fieldView.isArray())
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams, std::string{fieldView.key()} + "NotArray"}};
        }
        if (fieldView.arraySize() == 0)
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcActMalformed, std::string{fieldView.key()} + " malformed."}};
        }
        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            auto const elem = fieldView.element(i);
            if (not elem.isString())
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcInvalidParams,
                    std::string{fieldView.key()} + "'sItemNotString"}};
            }
            if (not rpc::spec::detail::accountFromStringStrict(std::string{elem.asString()}))
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcActMalformed,
                    std::string{fieldView.key()} + "'sItemMalformed"}};
            }
        }
        return {};
    }};

// The accepted set is server-conditional (the spec is shared):
//   - both servers serve the six common streams below;
//   - xrpld additionally accepts `server`/`peer_status`/`consensus` (the admin
//     role for the first two is enforced later, not here) and the deprecated
//     `rt_transactions` alias of `transactions_proposed`;
//   - Clio does not serve those, so it rejects them with RpcNotSupported.
// Errors otherwise mirror the old subscribeStreamValidator: not array →
// "<key>NotArray"; element not string → "streamNotString"; unknown → RpcStreamMalformed.
// A dedicated validator (rather than a CustomValidator lambda) so the accepted
// stream values are exposed to the schema dump via describeParams().
/**
 * @brief Validates the streams field.
 */
struct StreamsValidator
{
    /**
     * @brief Identifier for this item in the schema dump ("streams").
     */
    static constexpr std::string_view kName = "streams";

    // Streams both servers serve.
    /**
     * @brief Common.
     */
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
    // xrpld also accepts these (admin role enforced later); rt_transactions is a
    // deprecated alias for transactions_proposed.
    /**
     * @brief Rippled extra.
     */
    static constexpr std::array<std::string_view, 4> kRippledExtra{
        "server",
        "peer_status",
        "consensus",
        "rt_transactions"};
#endif

    /**
     * @brief Whether @p v is one of @p set.
     *
     * @param arr The set to search.
     * @param value The value to look for.
     * @return true when present; false otherwise.
     */
    static constexpr bool
    contains(auto const& arr, std::string_view value)
    {
        for (auto const& entry : arr)
        {
            if (entry == value)
                return true;
        }
        return false;
    }

    /**
     * @brief Render this item's parameters into the schema dump.
     *
     * @tparam Writer The dump-writer type.
     * @param writer The writer receiving the parameters.
     */
    template <typename Writer>
    void
    describeParams(Writer& writer) const
    {
        writer.paramList("oneOf", kCommon);
#if RPCSPEC_IS_CLIO
        writer.paramList("notSupported", kNotSupported);
#else
        writer.paramList("alsoAllowed", kRippledExtra);
#endif
    }

    /**
     * @brief Validate the field.
     *
     * @tparam View The field-view type supplied by the backend.
     * @param fieldView The field to check.
     * @return Empty on success; a Status describing the failure otherwise.
     */
    template <SomeFieldView View>
    [[nodiscard]] MaybeError
    verify(View const& fieldView) const
    {
        if (not fieldView.present())
            return {};
        if (not fieldView.isArray())
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams, std::string{fieldView.key()} + "NotArray"}};
        }
        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            auto const elem = fieldView.element(i);
            if (not elem.isString())
            {
                return std::unexpected{
                    rpc::Status{rpc::XrpldError::RpcInvalidParams, "streamNotString"}};
            }
            auto const str = elem.asString();
#if RPCSPEC_IS_CLIO
            if (contains(kNotSupported, str))
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcNotSupported}};
            if (not contains(kCommon, str))
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcStreamMalformed}};
#else
            if (not contains(kCommon, str) and not contains(kRippledExtra, str))
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcStreamMalformed}};
#endif
        }
        return {};
    }
};

// NOLINTNEXTLINE(readability-identifier-naming)
/**
 * @brief Validator instance: streams.
 */
inline constexpr auto kSubscribeStreamValidator = StreamsValidator{};

// Errors mirror the old kBooksValidator lambda exactly (including all parseBook errors).
// Note: Unsubscribe does NOT check snapshot (no snapshot field in unsubscribe).
/**
 * @brief Validator instance: custom.
 */
inline constexpr auto kBooksValidator = CustomValidator{[](auto const& fieldView) -> MaybeError {
    if (not fieldView.isArray())
    {
        return std::unexpected{rpc::Status{
            rpc::XrpldError::RpcInvalidParams, std::string{fieldView.key()} + "NotArray"}};
    }
    for (auto i = 0uz; i < fieldView.arraySize(); ++i)
    {
        auto const book = fieldView.element(i);
        if (not book.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams, std::string{fieldView.key()} + "ItemNotObject"}};
        }

        auto const bothView = book.child("both");
        if (bothView.present() and not bothView.isBool())
        {
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcInvalidParams, "bothNotBool"}};
        }

        auto const takerPaysView = book.child("taker_pays");
        if (not takerPaysView.present())
        {
            return std::unexpected{
                rpc::Status{rpc::XrpldError::RpcInvalidParams, "Missing field 'taker_pays'"}};
        }
        if (not takerPaysView.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams, "Field 'taker_pays' is not an object"}};
        }

        auto const takerGetsView = book.child("taker_gets");
        if (not takerGetsView.present())
        {
            return std::unexpected{
                rpc::Status{rpc::XrpldError::RpcInvalidParams, "Missing field 'taker_gets'"}};
        }
        if (not takerGetsView.isObject())
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcInvalidParams, "Field 'taker_gets' is not an object"}};
        }

        auto const paysCurView = takerPaysView.child("currency");
        if (not paysCurView.present() or not paysCurView.isString())
        {
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcSrcCurMalformed}};
        }
        xrpl::Currency payCurrency;
        if (not xrpl::toCurrency(payCurrency, std::string{paysCurView.asString()}))
        {
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcSrcCurMalformed}};
        }

        auto const getsCurView = takerGetsView.child("currency");
        if (not getsCurView.present() or not getsCurView.isString())
        {
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcDstAmtMalformed}};
        }
        xrpl::Currency getCurrency;
        if (not xrpl::toCurrency(getCurrency, std::string{getsCurView.asString()}))
        {
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcDstAmtMalformed}};
        }

        // book-level domain (mirrors parseBook): must be string if present
        auto const domainView = book.child("domain");
        if (domainView.present() and not domainView.isString())
        {
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcDomainMalformed}};
        }

        xrpl::AccountID payIssuer;
        auto const paysIssuerView = takerPaysView.child("issuer");
        if (paysIssuerView.present())
        {
            if (not paysIssuerView.isString())
            {
                return std::unexpected{
                    rpc::Status{rpc::XrpldError::RpcInvalidParams, "takerPaysIssuerNotString"}};
            }
            if (not xrpl::toIssuer(payIssuer, std::string{paysIssuerView.asString()}))
            {
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcSrcIsrMalformed}};
            }
            if (payIssuer == xrpl::noAccount())
            {
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcSrcIsrMalformed}};
            }
        }
        else
        {
            payIssuer = xrpl::xrpAccount();
        }

        if (xrpl::isXRP(payCurrency) and not xrpl::isXRP(payIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcSrcIsrMalformed,
                "Unneeded field 'taker_pays.issuer' for XRP currency specification."}};
        }
        if (not xrpl::isXRP(payCurrency) and xrpl::isXRP(payIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcSrcIsrMalformed,
                "Invalid field 'taker_pays.issuer', expected non-XRP issuer."}};
        }

        xrpl::AccountID getIssuer;
        auto const getsIssuerView = takerGetsView.child("issuer");
        if (getsIssuerView.present())
        {
            if (not getsIssuerView.isString())
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcInvalidParams, "taker_gets.issuer should be string"}};
            }
            if (not xrpl::toIssuer(getIssuer, std::string{getsIssuerView.asString()}))
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcDstIsrMalformed,
                    "Invalid field 'taker_gets.issuer', bad issuer."}};
            }
            if (getIssuer == xrpl::noAccount())
            {
                return std::unexpected{rpc::Status{
                    rpc::XrpldError::RpcDstIsrMalformed,
                    "Invalid field 'taker_gets.issuer', bad issuer account one."}};
            }
        }
        else
        {
            getIssuer = xrpl::xrpAccount();
        }

        if (xrpl::isXRP(getCurrency) and not xrpl::isXRP(getIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcDstIsrMalformed,
                "Unneeded field 'taker_gets.issuer' for XRP currency specification."}};
        }
        if (not xrpl::isXRP(getCurrency) and xrpl::isXRP(getIssuer))
        {
            return std::unexpected{rpc::Status{
                rpc::XrpldError::RpcDstIsrMalformed,
                "Invalid field 'taker_gets.issuer', expected non-XRP issuer."}};
        }

        if (payCurrency == getCurrency and payIssuer == getIssuer)
        {
            return std::unexpected{rpc::Status{rpc::XrpldError::RpcBadMarket}};
        }

        // book-level domain (mirrors inner parseBook overload): must parse as hex
        if (domainView.present())
        {
            xrpl::uint256 dom;
            if (not dom.parseHex(std::string{domainView.asString()}))
            {
                return std::unexpected{rpc::Status{rpc::XrpldError::RpcDomainMalformed}};
            }
        }
    }
    return {};
}};

/**
 * @brief Converts the stream vec field into its strongly-typed value.
 */
struct StreamVecConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("streamVec").
     */
    static constexpr std::string_view kName = "streamVec";

    /**
     * @brief The value this converter produces (`std::optional<std::vector<StreamType>>`).
     */
    using ValueType = std::optional<std::vector<StreamType>>;

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
        // The streams validator already rejected unknown/unsupported names, so each
        // element here is an accepted stream for this server build.
        std::vector<StreamType> result;
        result.reserve(fieldView.arraySize());
        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            auto const text = fieldView.element(i).asString();
            if (text == "ledger")
            {
                result.push_back(StreamType::Ledger);
            }
            else if (text == "transactions")
            {
                result.push_back(StreamType::Transactions);
            }
            else if (text == "transactions_proposed" or text == "rt_transactions")
            {  // rt_transactions: deprecated alias
                result.push_back(StreamType::TransactionsProposed);
            }
            else if (text == "book_changes")
            {
                result.push_back(StreamType::BookChanges);
            }
            else if (text == "manifests")
            {
                result.push_back(StreamType::Manifests);
            }
            else if (text == "validations")
            {
                result.push_back(StreamType::Validations);
            }
            else if (text == "server")
            {
                result.push_back(StreamType::Server);
            }
            else if (text == "peer_status")
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

/**
 * @brief Converts the unsubscribe books field into its strongly-typed value.
 */
struct UnsubscribeBooksConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("unsubscribeBooksVec").
     */
    static constexpr std::string_view kName = "unsubscribeBooksVec";

    /**
     * @brief The value this converter produces (`std::optional<std::vector<OrderBook>>`).
     */
    using ValueType = std::optional<std::vector<OrderBook>>;

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
        std::vector<OrderBook> result;
        result.reserve(fieldView.arraySize());

        for (auto i = 0uz; i < fieldView.arraySize(); ++i)
        {
            auto const bookView = fieldView.element(i);
            OrderBook ob;

            auto const bothView = bookView.child("both");
            if (bothView.present())
                ob.both = bothView.asBool();

            auto const paysView = bookView.child("taker_pays");
            auto const getsView = bookView.child("taker_gets");

            auto const payCurrency = rpc::spec::detail::currencyFromValidated(
                std::string{paysView.child("currency").asString()});
            auto const getCurrency = rpc::spec::detail::currencyFromValidated(
                std::string{getsView.child("currency").asString()});

            auto const paysIssuerView = paysView.child("issuer");
            xrpl::AccountID const payIssuer = paysIssuerView.present()
                ? rpc::spec::detail::issuerFromValidated(std::string{paysIssuerView.asString()})
                : xrpl::xrpAccount();

            auto const getsIssuerView = getsView.child("issuer");
            xrpl::AccountID const getIssuer = getsIssuerView.present()
                ? rpc::spec::detail::issuerFromValidated(std::string{getsIssuerView.asString()})
                : xrpl::xrpAccount();

            std::optional<xrpl::uint256> domainID;
            auto const domainView = bookView.child("domain");
            if (domainView.present())
            {
                domainID =
                    rpc::spec::detail::uint256FromValidated(std::string{domainView.asString()});
            }

            ob.book = xrpl::Book{
                xrpl::Issue{payCurrency, payIssuer}, xrpl::Issue{getCurrency, getIssuer}, domainID};

            result.push_back(ob);  // OrderBook is trivially copyable here
        }

        return std::optional<std::vector<OrderBook>>{std::move(result)};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: stream vec.
 */
inline constexpr auto streamVecConv = StreamVecConverter{};

/**
 * @brief Converter instance: unsubscribe books.
 */
inline constexpr auto unsubscribeBooksConv = UnsubscribeBooksConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    field("streams", &Input::streams, kSubscribeStreamValidator, streamVecConv),
    field("accounts", &Input::accounts, kSubscribeAccountsValidator, asAccountIdVec),
    field(
        "accounts_proposed",
        &Input::accountsProposed,
        kSubscribeAccountsValidator,
        asAccountIdVec),
    field("books", &Input::books, kBooksValidator, unsubscribeBooksConv),
    field("url") | deprecated,
    field("rt_accounts") | deprecated,
    field("rt_transactions") | deprecated);

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

}  // namespace rpc::spec::handlers::unsubscribe
