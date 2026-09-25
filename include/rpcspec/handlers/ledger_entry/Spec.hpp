/** @file */
#pragma once

#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/UintTypes.h>

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/VersionedSpec.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rpc::spec::handlers::ledger_entry {

/**
 * @brief Validator for the ripple state accounts field.
 */
inline constexpr auto kRippleStateAccountsValidator =
    CustomValidator{[](auto const& fieldView) -> MaybeError {
        if (not fieldView.isArray() or fieldView.arraySize() != 2)
        {
            return std::unexpected{
                rpc::Status{rpc::XrpldError::RpcInvalidParams, "malformedAccounts"}};
        }
        auto const elem0 = fieldView.element(0);
        auto const elem1 = fieldView.element(1);
        if (not elem0.isString() or not elem1.isString() or elem0.asString() == elem1.asString())
        {
            return std::unexpected{
                rpc::Status{rpc::XrpldError::RpcInvalidParams, "malformedAccounts"}};
        }
        auto const id1 =
            rpc::spec::detail::parseBase58Wrapper<xrpl::AccountID>(std::string{elem0.asString()});
        auto const id2 =
            rpc::spec::detail::parseBase58Wrapper<xrpl::AccountID>(std::string{elem1.asString()});
        if (not id1 or not id2)
        {
            return std::unexpected{rpc::Status{rpc::kMalformedAddress, "malformedAddresses"}};
        }
        return {};
    }};

/**
 * @brief Validator for the malformed request hex string field.
 */
inline constexpr auto kMalformedRequestHexStringValidator =
    withCustomError(uint256Hex, rpc::kMalformedRequest);

/**
 * @brief Validator for the malformed request int field.
 */
inline constexpr auto kMalformedRequestIntValidator =
    withCustomError(type<uint32_t>, rpc::kMalformedRequest);

/**
 * @brief Validator for the bridge json field.
 */
inline constexpr auto kBridgeJsonValidator = withCustomError(
    ifType<JsonObject>(section(
        field("LockingChainDoor", required, accountBase58),
        field("IssuingChainDoor", required, accountBase58),
        field("LockingChainIssue", required, currencyIssue),
        field("IssuingChainIssue", required, currencyIssue))),
    rpc::kMalformedRequest);

/**
 * @brief Build an Issue from a validated currency/issuer object.
 *
 * @param fieldView A validated currency/issuer object.
 * @return The decoded issue.
 */
template <typename View>
inline xrpl::Issue
issueFromCurrencyIssue(View const& fieldView)
{
    auto const currency = rpc::spec::detail::currencyFromValidated(
        std::string{fieldView.child("currency").asString()});
    if (xrpl::isXRP(currency))
        return xrpl::Issue{currency, xrpl::AccountID{}};
    auto const issuer =
        rpc::spec::detail::issuerFromValidated(std::string{fieldView.child("issuer").asString()});
    return xrpl::Issue{currency, issuer};
}

/**
 * @brief Build a BridgeSpec from a validated bridge object.
 *
 * @param fieldView A validated bridge object.
 * @return The decoded bridge spec.
 */
template <typename View>
inline BridgeSpec
bridgeSpecFromObject(View const& fieldView)
{
    BridgeSpec bs;
    bs.lockingChainDoor = rpc::spec::detail::accountFromValidated(
        std::string{fieldView.child("LockingChainDoor").asString()});
    bs.issuingChainDoor = rpc::spec::detail::accountFromValidated(
        std::string{fieldView.child("IssuingChainDoor").asString()});
    bs.lockingChainIssue = issueFromCurrencyIssue(fieldView.child("LockingChainIssue"));
    bs.issuingChainIssue = issueFromCurrencyIssue(fieldView.child("IssuingChainIssue"));
    return bs;
}

/**
 * @brief Converts the directory field into its strongly-typed value.
 */
struct DirectoryConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("directory").
     */
    static constexpr std::string_view kName = "directory";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, DirectoryEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, DirectoryEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        DirectoryEntry entry;
        auto const ownerView = fieldView.child("owner");
        if (ownerView.present() and ownerView.isString())
        {
            entry.owner =
                rpc::spec::detail::accountFromValidated(std::string{ownerView.asString()});
        }
        auto const dirRootView = fieldView.child("dir_root");
        if (dirRootView.present() and dirRootView.isString())
        {
            entry.dirRoot =
                rpc::spec::detail::uint256FromValidated(std::string{dirRootView.asString()});
        }
        auto const subIndexView = fieldView.child("sub_index");
        if (subIndexView.present() and subIndexView.isUint32())
            entry.subIndex = subIndexView.asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the offer field into its strongly-typed value.
 */
struct OfferConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("offer").
     */
    static constexpr std::string_view kName = "offer";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, OfferEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, OfferEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        OfferEntry entry;
        entry.account = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("account").asString()});
        entry.seq = fieldView.child("seq").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the escrow field into its strongly-typed value.
 */
struct EscrowConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("escrow").
     */
    static constexpr std::string_view kName = "escrow";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, EscrowEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, EscrowEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        EscrowEntry entry;
        entry.owner = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("owner").asString()});
        entry.seq = fieldView.child("seq").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the ticket field into its strongly-typed value.
 */
struct TicketConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("ticket").
     */
    static constexpr std::string_view kName = "ticket";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, TicketEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, TicketEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        TicketEntry entry;
        entry.account = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("account").asString()});
        entry.ticketSeq = fieldView.child("ticket_seq").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the permissioned domain field into its strongly-typed value.
 */
struct PermissionedDomainConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("permissioned_domain").
     */
    static constexpr std::string_view kName = "permissioned_domain";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256,
     * PermissionedDomainEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, PermissionedDomainEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        PermissionedDomainEntry entry;
        entry.account = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("account").asString()});
        entry.seq = fieldView.child("seq").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the vault field into its strongly-typed value.
 */
struct VaultConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("vault").
     */
    static constexpr std::string_view kName = "vault";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, VaultEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, VaultEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        VaultEntry entry;
        entry.owner = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("owner").asString()});
        entry.seq = fieldView.child("seq").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the loan broker field into its strongly-typed value.
 */
struct LoanBrokerConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("loan_broker").
     */
    static constexpr std::string_view kName = "loan_broker";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, LoanBrokerEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, LoanBrokerEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        LoanBrokerEntry entry;
        entry.owner = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("owner").asString()});
        entry.seq = fieldView.child("seq").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the loan field into its strongly-typed value.
 */
struct LoanConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("loan").
     */
    static constexpr std::string_view kName = "loan";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, LoanEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, LoanEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        LoanEntry entry;
        entry.loanBrokerId = rpc::spec::detail::uint256FromValidated(
            std::string{fieldView.child("loan_broker_id").asString()});
        entry.loanSeq = fieldView.child("loan_seq").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the delegate field into its strongly-typed value.
 */
struct DelegateConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("delegate").
     */
    static constexpr std::string_view kName = "delegate";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, DelegateEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, DelegateEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        DelegateEntry entry;
        entry.account = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("account").asString()});
        entry.authorize = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("authorize").asString()});
        return ValueType{entry};
    }
};

/**
 * @brief Converts the sponsorship field into its strongly-typed value.
 */
struct SponsorshipConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("sponsorship").
     */
    static constexpr std::string_view kName = "sponsorship";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, SponsorshipEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, SponsorshipEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        SponsorshipEntry entry;
        entry.sponsor = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("sponsor").asString()});
        entry.sponsee = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("sponsee").asString()});
        return ValueType{entry};
    }
};

/**
 * @brief Converts the mptoken field into its strongly-typed value.
 */
struct MptokenConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("mptoken").
     */
    static constexpr std::string_view kName = "mptoken";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, MptokenEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, MptokenEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        MptokenEntry entry;
        entry.account = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("account").asString()});
        entry.mptIssuanceId = rpc::spec::detail::uint192FromValidated(
            std::string{fieldView.child("mpt_issuance_id").asString()});
        return ValueType{entry};
    }
};

/**
 * @brief Converts the amm field into its strongly-typed value.
 */
struct AmmConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("amm").
     */
    static constexpr std::string_view kName = "amm";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, AmmEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, AmmEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        AmmEntry entry;
        entry.asset = issueFromCurrencyIssue(fieldView.child("asset"));
        entry.asset2 = issueFromCurrencyIssue(fieldView.child("asset2"));
        return ValueType{entry};
    }
};

/**
 * @brief Converts the oracle field into its strongly-typed value.
 */
struct OracleConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("oracle").
     */
    static constexpr std::string_view kName = "oracle";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, OracleEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, OracleEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        OracleEntry entry;
        entry.account = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("account").asString()});
        entry.oracleDocumentId = fieldView.child("oracle_document_id").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the credential field into its strongly-typed value.
 */
struct CredentialConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("credential").
     */
    static constexpr std::string_view kName = "credential";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, CredentialEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, CredentialEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        CredentialEntry entry;
        entry.subject = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("subject").asString()});
        entry.issuer = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("issuer").asString()});
        entry.credentialType = std::string{fieldView.child("credential_type").asString()};
        return ValueType{entry};
    }
};

/**
 * @brief Converts the deposit preauth field into its strongly-typed value.
 */
struct DepositPreauthConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("deposit_preauth").
     */
    static constexpr std::string_view kName = "deposit_preauth";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256,
     * DepositPreauthEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, DepositPreauthEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        DepositPreauthEntry entry;
        entry.owner = rpc::spec::detail::accountFromValidated(
            std::string{fieldView.child("owner").asString()});
        auto const authView = fieldView.child("authorized");
        if (authView.present() and authView.isString())
        {
            entry.authorized =
                rpc::spec::detail::accountFromValidated(std::string{authView.asString()});
        }
        auto const credsView = fieldView.child("authorized_credentials");
        if (credsView.present() and credsView.isArray())
        {
            std::vector<AuthorizeCredentialEntry> creds;
            for (auto i = 0uz; i < credsView.arraySize(); ++i)
            {
                auto const elem = credsView.element(i);
                AuthorizeCredentialEntry ace;
                ace.issuer = rpc::spec::detail::accountFromValidated(
                    std::string{elem.child("issuer").asString()});
                ace.credentialType = std::string{elem.child("credential_type").asString()};
                creds.push_back(ace);
            }
            entry.authorizedCredentials = std::move(creds);
        }
        return ValueType{entry};
    }
};

/**
 * @brief Converts the ripple state field into its strongly-typed value.
 */
struct RippleStateConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("ripple_state").
     */
    static constexpr std::string_view kName = "ripple_state";

    /**
     * @brief The value this converter produces (a hex key or `RippleStateEntry`).
     */
    using ValueType = std::variant<xrpl::uint256, RippleStateEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};

        RippleStateEntry entry;
        auto const accountsView = fieldView.child("accounts");
        entry.accounts[0] = rpc::spec::detail::accountFromValidated(
            std::string{accountsView.element(0).asString()});
        entry.accounts[1] = rpc::spec::detail::accountFromValidated(
            std::string{accountsView.element(1).asString()});
        entry.currency = rpc::spec::detail::currencyFromValidated(
            std::string{fieldView.child("currency").asString()});
        return entry;
    }
};

/**
 * @brief Converts the bridge field into its strongly-typed value.
 */
struct BridgeConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("bridge").
     */
    static constexpr std::string_view kName = "bridge";

    /**
     * @brief The value this converter produces (`BridgeSpec`).
     */
    using ValueType = BridgeSpec;

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
        return bridgeSpecFromObject(fieldView);
    }
};

/**
 * @brief Converts the x chain claim id field into its strongly-typed value.
 */
struct XChainClaimIdConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("xchain_owned_claim_id").
     */
    static constexpr std::string_view kName = "xchain_owned_claim_id";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, XChainClaimIdEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, XChainClaimIdEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        XChainClaimIdEntry entry;
        entry.bridge = bridgeSpecFromObject(fieldView);
        entry.claimId = fieldView.child("xchain_owned_claim_id").asUint32();
        return ValueType{entry};
    }
};

/**
 * @brief Converts the x chain create account claim id field into its strongly-typed value.
 */
struct XChainCreateAccountClaimIdConverter
{
    /**
     * @brief Identifier for this item in the schema dump ("xchain_owned_create_account_claim_id").
     */
    static constexpr std::string_view kName = "xchain_owned_create_account_claim_id";

    /**
     * @brief The value this converter produces (`std::variant<xrpl::uint256, XChainClaimIdEntry>`).
     */
    using ValueType = std::variant<xrpl::uint256, XChainClaimIdEntry>;

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
            return ValueType{
                rpc::spec::detail::uint256FromValidated(std::string{fieldView.asString()})};
        }
        XChainClaimIdEntry entry;
        entry.bridge = bridgeSpecFromObject(fieldView);
        entry.claimId = fieldView.child("xchain_owned_create_account_claim_id").asUint32();
        return ValueType{entry};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
/**
 * @brief Converter instance: directory.
 */
inline constexpr auto directoryConv = DirectoryConverter{};

/**
 * @brief Converter instance: offer.
 */
inline constexpr auto offerConv = OfferConverter{};

/**
 * @brief Converter instance: escrow.
 */
inline constexpr auto escrowConv = EscrowConverter{};

/**
 * @brief Converter instance: ticket.
 */
inline constexpr auto ticketConv = TicketConverter{};

/**
 * @brief Converter instance: permissioned domain.
 */
inline constexpr auto permissionedDomainConv = PermissionedDomainConverter{};

/**
 * @brief Converter instance: vault.
 */
inline constexpr auto vaultConv = VaultConverter{};

/**
 * @brief Converter instance: loan broker.
 */
inline constexpr auto loanBrokerConv = LoanBrokerConverter{};

/**
 * @brief Converter instance: loan.
 */
inline constexpr auto loanConv = LoanConverter{};

/**
 * @brief Converter instance: delegate.
 */
inline constexpr auto delegateConv = DelegateConverter{};

/**
 * @brief Converter instance: sponsorship.
 */
inline constexpr auto sponsorshipConv = SponsorshipConverter{};

/**
 * @brief Converter instance: mptoken.
 */
inline constexpr auto mptokenConv = MptokenConverter{};

/**
 * @brief Converter instance: amm.
 */
inline constexpr auto ammConv = AmmConverter{};

/**
 * @brief Converter instance: oracle.
 */
inline constexpr auto oracleConv = OracleConverter{};

/**
 * @brief Converter instance: credential.
 */
inline constexpr auto credentialConv = CredentialConverter{};

/**
 * @brief Converter instance: deposit preauth.
 */
inline constexpr auto depositPreauthConv = DepositPreauthConverter{};

/**
 * @brief Converter instance: ripple state.
 */
inline constexpr auto rippleStateConv = RippleStateConverter{};

/**
 * @brief Converter instance: bridge.
 */
inline constexpr auto bridgeConv = BridgeConverter{};

/**
 * @brief Converter instance: x chain claim id.
 */
inline constexpr auto xChainClaimIdConv = XChainClaimIdConverter{};

/**
 * @brief Converter instance: x chain create account claim id.
 */
inline constexpr auto xChainCreateAccountClaimIdConv = XChainCreateAccountClaimIdConverter{};
// NOLINTEND(readability-identifier-naming)

/**
 * @brief A locator with two accepted spellings and a single Input member.
 *
 * Reuses the field's validators and converter for either spelling. Supplying both
 * names is ambiguous and rejected, matching xrpld's locator selection.
 *
 * @tparam Field The bound locator field.
 */
template <typename Field>
struct AliasedLocator : Field
{
    /** @brief The additional request field name. */
    std::string_view alias;

    /**
     * @brief Parse either spelling, rejecting requests containing both.
     * @param root Request object view.
     * @param out Input to populate.
     * @return An error for conflicting names or an invalid field value.
     */
    template <SomeObjectView Root>
    [[nodiscard]] MaybeError
    parseInto(Root& root, Input& out) const
    {
        auto selected = static_cast<Field const&>(*this);
        if (root.child(alias).present())
        {
            if (root.child(this->key).present())
                return std::unexpected{
                    rpc::Status{rpc::XrpldError::RpcInvalidParams, "Too many fields provided."}};
            selected.key = alias;
        }
        return selected.parseInto(root, out);
    }

    /**
     * @brief Collect warnings using the spelling present in the request.
     * @param root Request object view.
     * @return Warnings from the selected field.
     */
    template <SomeObjectView Root>
    [[nodiscard]] Warnings
    check(Root const& root) const
    {
        auto selected = static_cast<Field const&>(*this);
        if (root.child(alias).present())
            selected.key = alias;
        return selected.check(root);
    }

    /**
     * @brief Include both spellings in the schema.
     * @param writer Schema output writer.
     */
    void
    dump(SpecDumpWriter& writer) const
    {
        Field::dump(writer);
        auto alternate = static_cast<Field const&>(*this);
        alternate.key = alias;
        alternate.dump(writer);
    }
};

/**
 * @brief The spec that validates a request and parses it into `Input`.
 */
inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger),
    field("binary", &Input::binary, type<bool>, jsonBool),
    field("index", &Input::index, kMalformedRequestHexStringValidator, asUint256),
    AliasedLocator{field("account_root", &Input::accountRoot, accountBase58, accountId), "account"},
    field("did", &Input::did, accountBase58, accountId),
    field("check", &Input::check, kMalformedRequestHexStringValidator, asUint256),
    field(
        "deposit_preauth",
        &Input::depositPreauth,
        type<std::string, JsonObject>,
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field("owner", required, withCustomError(accountBase58, rpc::kMalformedOwner)),
            field("authorized", accountBase58),
            field("authorized_credentials", authorizeCredential))),
        depositPreauthConv),
    field(
        "directory",
        &Input::directory,
        type<std::string, JsonObject>,
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field("owner", accountBase58),
            field("dir_root", uint256Hex),
            field("sub_index", kMalformedRequestIntValidator))),
        directoryConv),
    field(
        "escrow",
        &Input::escrow,
        type<std::string, JsonObject>,
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field("owner", required, withCustomError(accountBase58, rpc::kMalformedOwner)),
            field("seq", required, kMalformedRequestIntValidator))),
        escrowConv),
    field(
        "offer",
        &Input::offer,
        type<std::string, JsonObject>,
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field("account", required, accountBase58),
            field("seq", required, kMalformedRequestIntValidator))),
        offerConv),
    field(
        "payment_channel",
        &Input::paymentChannel,
        kMalformedRequestHexStringValidator,
        asUint256),
    AliasedLocator{
        field(
            "ripple_state",
            &Input::rippleStateAccount,
            type<std::string, JsonObject>,
            ifType<std::string>(kMalformedRequestHexStringValidator),
            ifType<JsonObject>(section(
                field("accounts", required, kRippleStateAccountsValidator),
                field("currency", required, currency))),
            rippleStateConv),
        "state"},
    field(
        "ticket",
        &Input::ticket,
        type<std::string, JsonObject>,
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field("account", required, accountBase58),
            field("ticket_seq", required, kMalformedRequestIntValidator))),
        ticketConv),
    field("nft_page", &Input::nftPage, kMalformedRequestHexStringValidator, asUint256),
    field(
        "amm",
        &Input::amm,
        type<std::string, JsonObject>,
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "asset",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<JsonObject>, rpc::kMalformedRequest),
                currencyIssue),
            field(
                "asset2",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<JsonObject>, rpc::kMalformedRequest),
                currencyIssue))),
        ammConv),
    field(
        "bridge",
        &Input::bridge,
        withCustomError(type<JsonObject>, rpc::kMalformedRequest),
        kBridgeJsonValidator,
        bridgeConv),
    field(
        "bridge_account",
        &Input::bridgeAccount,
        withCustomError(accountBase58, rpc::kMalformedRequest),
        accountId),
    field(
        "xchain_owned_claim_id",
        &Input::xchainOwnedClaimId,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        kBridgeJsonValidator,
        withCustomError(
            ifType<JsonObject>(section(field("xchain_owned_claim_id", required, type<uint32_t>))),
            rpc::kMalformedRequest),
        xChainClaimIdConv),
    field(
        "xchain_owned_create_account_claim_id",
        &Input::xchainOwnedCreateAccountClaimId,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        kBridgeJsonValidator,
        withCustomError(
            ifType<JsonObject>(
                section(field("xchain_owned_create_account_claim_id", required, type<uint32_t>))),
            rpc::kMalformedRequest),
        xChainCreateAccountClaimIdConv),
    field(
        "oracle",
        &Input::oracle,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(
            withCustomError(kMalformedRequestHexStringValidator, rpc::kMalformedAddress)),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)),
            // note: Unlike `xrpld`, Clio only supports UInt as input, no string, no
            // `null`, etc.:
            field(
                "oracle_document_id",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<uint32_t, std::string>, rpc::kMalformedOracleDocumentId),
                withCustomError(toNumber, rpc::kMalformedOracleDocumentId)))),
        oracleConv),
    field(
        "credential",
        &Input::credential,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(
            withCustomError(kMalformedRequestHexStringValidator, rpc::kMalformedAddress)),
        ifType<JsonObject>(section(
            field(
                "subject",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)),
            field(
                "issuer",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)),
            field(
                "credential_type",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<std::string>, rpc::kMalformedRequest),
                credentialType))),
        credentialConv),
    field(
        "mpt_issuance",
        &Input::mptIssuance,
        withCustomError(uint192Hex, rpc::kMalformedRequest),
        asUint192),
    field(
        "mptoken",
        &Input::mptoken,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)),
            field(
                "mpt_issuance_id",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(uint192Hex, rpc::kMalformedRequest)))),
        mptokenConv),
    field(
        "permissioned_domain",
        &Input::permissionedDomain,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<uint32_t>, rpc::kMalformedRequest)),
            field(
                "account",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)))),
        permissionedDomainConv),
    field(
        "vault",
        &Input::vault,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<uint32_t>, rpc::kMalformedRequest)),
            field(
                "owner",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedOwner)))),
        vaultConv),
    field(
        "loan_broker",
        &Input::loanBroker,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<uint32_t>, rpc::kMalformedRequest)),
            field(
                "owner",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedOwner)))),
        loanBrokerConv),
    field(
        "loan",
        &Input::loan,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "loan_seq",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(type<uint32_t>, rpc::kMalformedRequest)),
            field(
                "loan_broker_id",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(uint256Hex, rpc::kMalformedRequest)))),
        loanConv),
    field(
        "delegate",
        &Input::delegate,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)),
            field(
                "authorize",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)))),
        delegateConv),
    field(
        "sponsorship",
        &Input::sponsorship,
        withCustomError(type<std::string, JsonObject>, rpc::kMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "sponsor",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)),
            field(
                "sponsee",
                withCustomError(required, rpc::kMalformedRequest),
                withCustomError(accountBase58, rpc::kMalformedAddress)))),
        sponsorshipConv),
    field("amendments", &Input::amendments, kMalformedRequestHexStringValidator, asUint256),
    field("fee", &Input::fee, kMalformedRequestHexStringValidator, asUint256),
    field("hashes", &Input::hashes, kMalformedRequestHexStringValidator, asUint256),
    field("nft_offer", &Input::nftOffer, kMalformedRequestHexStringValidator, asUint256),
    field("nunl", &Input::nunl, kMalformedRequestHexStringValidator, asUint256),
    field("signer_list", &Input::signerList, kMalformedRequestHexStringValidator, asUint256),
    field("ledger", deprecated),
    field("include_deleted", &Input::includeDeleted, type<bool>, jsonBool));

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

}  // namespace rpc::spec::handlers::ledger_entry
