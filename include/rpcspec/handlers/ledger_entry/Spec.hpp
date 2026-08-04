/** @file */
#pragma once
// Shared constexpr spec for the 'ledger_entry' RPC command.
// Single source of truth — both Clio and xrpld include this file.

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
#include <variant>
#include <vector>

namespace rpc::spec::handlers::ledger_entry {

inline constexpr auto kRippleStateAccountsValidator =
    CustomValidator{[](auto const& f) -> MaybeError {
        if (!f.isArray() || f.arraySize() != 2)
        {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "malformedAccounts"}};
        }
        auto const elem0 = f.element(0);
        auto const elem1 = f.element(1);
        if (!elem0.isString() || !elem1.isString() || elem0.asString() == elem1.asString())
        {
            return std::unexpected{
                rpc::Status{rpc::RippledError::RpcInvalidParams, "malformedAccounts"}};
        }
        auto const id1 =
            rpc::spec::detail::parseBase58Wrapper<xrpl::AccountID>(std::string{elem0.asString()});
        auto const id2 =
            rpc::spec::detail::parseBase58Wrapper<xrpl::AccountID>(std::string{elem1.asString()});
        if (!id1 || !id2)
        {
            return std::unexpected{
                rpc::Status{rpc::ClioError::RpcMalformedAddress, "malformedAddresses"}};
        }
        return {};
    }};

inline constexpr auto kMalformedRequestHexStringValidator =
    withCustomError(uint256Hex, rpc::ClioError::RpcMalformedRequest);

inline constexpr auto kMalformedRequestIntValidator =
    withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest);

inline constexpr auto kBridgeJsonValidator = withCustomError(
    ifType<JsonObject>(section(
        field("LockingChainDoor", required, accountBase58),
        field("IssuingChainDoor", required, accountBase58),
        field("LockingChainIssue", required, currencyIssue),
        field("IssuingChainIssue", required, currencyIssue))),
    rpc::ClioError::RpcMalformedRequest);

template <typename FA>
inline xrpl::Issue
issueFromCurrencyIssue(FA const& fa)
{
    auto const currency =
        rpc::spec::detail::currencyFromValidated(std::string{fa.child("currency").asString()});
    if (xrpl::isXRP(currency))
        return xrpl::Issue{.currency = currency, .account = xrpl::AccountID{}};
    auto const issuer =
        rpc::spec::detail::issuerFromValidated(std::string{fa.child("issuer").asString()});
    return xrpl::Issue{.currency = currency, .account = issuer};
}

template <typename FA>
inline BridgeSpec
bridgeSpecFromObject(FA const& fa)
{
    BridgeSpec bs;
    bs.lockingChainDoor = rpc::spec::detail::accountFromValidated(
        std::string{fa.child("LockingChainDoor").asString()});
    bs.issuingChainDoor = rpc::spec::detail::accountFromValidated(
        std::string{fa.child("IssuingChainDoor").asString()});
    bs.lockingChainIssue = issueFromCurrencyIssue(fa.child("LockingChainIssue"));
    bs.issuingChainIssue = issueFromCurrencyIssue(fa.child("IssuingChainIssue"));
    return bs;
}

struct DirectoryConverter
{
    static constexpr std::string_view kName = "directory";
    using ValueType = std::variant<xrpl::uint256, DirectoryEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        DirectoryEntry entry;
        auto const ownerFa = f.child("owner");
        if (ownerFa.present() && ownerFa.isString())
            entry.owner = rpc::spec::detail::accountFromValidated(std::string{ownerFa.asString()});
        auto const dirRootFa = f.child("dir_root");
        if (dirRootFa.present() && dirRootFa.isString())
        {
            entry.dirRoot =
                rpc::spec::detail::uint256FromValidated(std::string{dirRootFa.asString()});
        }
        auto const subIndexFa = f.child("sub_index");
        if (subIndexFa.present() && subIndexFa.isUint32())
            entry.subIndex = subIndexFa.asUint32();
        return ValueType{entry};
    }
};

struct OfferConverter
{
    static constexpr std::string_view kName = "offer";
    using ValueType = std::variant<xrpl::uint256, OfferEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        OfferEntry entry;
        entry.account =
            rpc::spec::detail::accountFromValidated(std::string{f.child("account").asString()});
        entry.seq = f.child("seq").asUint32();
        return ValueType{entry};
    }
};

struct EscrowConverter
{
    static constexpr std::string_view kName = "escrow";
    using ValueType = std::variant<xrpl::uint256, EscrowEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        EscrowEntry entry;
        entry.owner =
            rpc::spec::detail::accountFromValidated(std::string{f.child("owner").asString()});
        entry.seq = f.child("seq").asUint32();
        return ValueType{entry};
    }
};

struct TicketConverter
{
    static constexpr std::string_view kName = "ticket";
    using ValueType = std::variant<xrpl::uint256, TicketEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        TicketEntry entry;
        entry.account =
            rpc::spec::detail::accountFromValidated(std::string{f.child("account").asString()});
        entry.ticketSeq = f.child("ticket_seq").asUint32();
        return ValueType{entry};
    }
};

struct PermissionedDomainConverter
{
    static constexpr std::string_view kName = "permissioned_domain";
    using ValueType = std::variant<xrpl::uint256, PermissionedDomainEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        PermissionedDomainEntry entry;
        entry.account =
            rpc::spec::detail::accountFromValidated(std::string{f.child("account").asString()});
        entry.seq = f.child("seq").asUint32();
        return ValueType{entry};
    }
};

struct VaultConverter
{
    static constexpr std::string_view kName = "vault";
    using ValueType = std::variant<xrpl::uint256, VaultEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        VaultEntry entry;
        entry.owner =
            rpc::spec::detail::accountFromValidated(std::string{f.child("owner").asString()});
        entry.seq = f.child("seq").asUint32();
        return ValueType{entry};
    }
};

struct LoanBrokerConverter
{
    static constexpr std::string_view kName = "loan_broker";
    using ValueType = std::variant<xrpl::uint256, LoanBrokerEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        LoanBrokerEntry entry;
        entry.owner =
            rpc::spec::detail::accountFromValidated(std::string{f.child("owner").asString()});
        entry.seq = f.child("seq").asUint32();
        return ValueType{entry};
    }
};

struct LoanConverter
{
    static constexpr std::string_view kName = "loan";
    using ValueType = std::variant<xrpl::uint256, LoanEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        LoanEntry entry;
        entry.loanBrokerId = rpc::spec::detail::uint256FromValidated(
            std::string{f.child("loan_broker_id").asString()});
        entry.loanSeq = f.child("loan_seq").asUint32();
        return ValueType{entry};
    }
};

struct DelegateConverter
{
    static constexpr std::string_view kName = "delegate";
    using ValueType = std::variant<xrpl::uint256, DelegateEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        DelegateEntry entry;
        entry.account =
            rpc::spec::detail::accountFromValidated(std::string{f.child("account").asString()});
        entry.authorize =
            rpc::spec::detail::accountFromValidated(std::string{f.child("authorize").asString()});
        return ValueType{entry};
    }
};

struct MptokenConverter
{
    static constexpr std::string_view kName = "mptoken";
    using ValueType = std::variant<xrpl::uint256, MptokenEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        MptokenEntry entry;
        entry.account =
            rpc::spec::detail::accountFromValidated(std::string{f.child("account").asString()});
        entry.mptIssuanceId = rpc::spec::detail::uint192FromValidated(
            std::string{f.child("mpt_issuance_id").asString()});
        return ValueType{entry};
    }
};

struct AmmConverter
{
    static constexpr std::string_view kName = "amm";
    using ValueType = std::variant<xrpl::uint256, AmmEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        AmmEntry entry;
        entry.asset = issueFromCurrencyIssue(f.child("asset"));
        entry.asset2 = issueFromCurrencyIssue(f.child("asset2"));
        return ValueType{entry};
    }
};

struct OracleConverter
{
    static constexpr std::string_view kName = "oracle";
    using ValueType = std::variant<xrpl::uint256, OracleEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        OracleEntry entry;
        entry.account =
            rpc::spec::detail::accountFromValidated(std::string{f.child("account").asString()});
        entry.oracleDocumentId = f.child("oracle_document_id").asUint32();
        return ValueType{entry};
    }
};

struct CredentialConverter
{
    static constexpr std::string_view kName = "credential";
    using ValueType = std::variant<xrpl::uint256, CredentialEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        CredentialEntry entry;
        entry.subject =
            rpc::spec::detail::accountFromValidated(std::string{f.child("subject").asString()});
        entry.issuer =
            rpc::spec::detail::accountFromValidated(std::string{f.child("issuer").asString()});
        entry.credentialType = std::string{f.child("credential_type").asString()};
        return ValueType{entry};
    }
};

struct DepositPreauthConverter
{
    static constexpr std::string_view kName = "deposit_preauth";
    using ValueType = std::variant<xrpl::uint256, DepositPreauthEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        DepositPreauthEntry entry;
        entry.owner =
            rpc::spec::detail::accountFromValidated(std::string{f.child("owner").asString()});
        auto const authFa = f.child("authorized");
        if (authFa.present() && authFa.isString())
        {
            entry.authorized =
                rpc::spec::detail::accountFromValidated(std::string{authFa.asString()});
        }
        auto const credsFa = f.child("authorized_credentials");
        if (credsFa.present() && credsFa.isArray())
        {
            std::vector<AuthorizeCredentialEntry> creds;
            for (std::size_t i = 0; i < credsFa.arraySize(); ++i)
            {
                auto const elem = credsFa.element(i);
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

struct RippleStateConverter
{
    static constexpr std::string_view kName = "ripple_state";
    using ValueType = RippleStateEntry;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        RippleStateEntry entry;
        auto const accountsFa = f.child("accounts");
        entry.accounts[0] =
            rpc::spec::detail::accountFromValidated(std::string{accountsFa.element(0).asString()});
        entry.accounts[1] =
            rpc::spec::detail::accountFromValidated(std::string{accountsFa.element(1).asString()});
        entry.currency =
            rpc::spec::detail::currencyFromValidated(std::string{f.child("currency").asString()});
        return entry;
    }
};

struct BridgeConverter
{
    static constexpr std::string_view kName = "bridge";
    using ValueType = BridgeSpec;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        return bridgeSpecFromObject(f);
    }
};

struct XChainClaimIdConverter
{
    static constexpr std::string_view kName = "xchain_owned_claim_id";
    using ValueType = std::variant<xrpl::uint256, XChainClaimIdEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        XChainClaimIdEntry entry;
        entry.bridge = bridgeSpecFromObject(f);
        entry.claimId = f.child("xchain_owned_claim_id").asUint32();
        return ValueType{entry};
    }
};

struct XChainCreateAccountClaimIdConverter
{
    static constexpr std::string_view kName = "xchain_owned_create_account_claim_id";
    using ValueType = std::variant<xrpl::uint256, XChainClaimIdEntry>;

    template <SomeFieldView FA>
    [[nodiscard]] Parsed<ValueType>
    parse(FA const& f) const
    {
        if (f.isString())
            return ValueType{rpc::spec::detail::uint256FromValidated(std::string{f.asString()})};
        XChainClaimIdEntry entry;
        entry.bridge = bridgeSpecFromObject(f);
        entry.claimId = f.child("xchain_owned_create_account_claim_id").asUint32();
        return ValueType{entry};
    }
};

// NOLINTBEGIN(readability-identifier-naming)
inline constexpr auto directoryConv = DirectoryConverter{};
inline constexpr auto offerConv = OfferConverter{};
inline constexpr auto escrowConv = EscrowConverter{};
inline constexpr auto ticketConv = TicketConverter{};
inline constexpr auto permissionedDomainConv = PermissionedDomainConverter{};
inline constexpr auto vaultConv = VaultConverter{};
inline constexpr auto loanBrokerConv = LoanBrokerConverter{};
inline constexpr auto loanConv = LoanConverter{};
inline constexpr auto delegateConv = DelegateConverter{};
inline constexpr auto mptokenConv = MptokenConverter{};
inline constexpr auto ammConv = AmmConverter{};
inline constexpr auto oracleConv = OracleConverter{};
inline constexpr auto credentialConv = CredentialConverter{};
inline constexpr auto depositPreauthConv = DepositPreauthConverter{};
inline constexpr auto rippleStateConv = RippleStateConverter{};
inline constexpr auto bridgeConv = BridgeConverter{};
inline constexpr auto xChainClaimIdConv = XChainClaimIdConverter{};
inline constexpr auto xChainCreateAccountClaimIdConv = XChainCreateAccountClaimIdConverter{};
// NOLINTEND(readability-identifier-naming)

inline constexpr auto kInputSpec = spec<Input>(
    ledgerSelector(&Input::ledger, withLegacyLedgerField),
    field("binary", &Input::binary, type<bool>, jsonBool),
    field("index", &Input::index, kMalformedRequestHexStringValidator, asUint256),
    field("account_root", &Input::accountRoot, accountBase58, accountId),
    field("did", &Input::did, accountBase58, accountId),
    field("check", &Input::check, kMalformedRequestHexStringValidator, asUint256),
    field(
        "deposit_preauth",
        &Input::depositPreauth,
        type<std::string, JsonObject>,
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "owner",
                required,
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)),
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
            field(
                "owner",
                required,
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)),
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
    field(
        "ripple_state",
        &Input::rippleStateAccount,
        type<JsonObject>,
        section(
            field("accounts", required, kRippleStateAccountsValidator),
            field("currency", required, currency)),
        rippleStateConv),
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
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<JsonObject>, rpc::ClioError::RpcMalformedRequest),
                currencyIssue),
            field(
                "asset2",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<JsonObject>, rpc::ClioError::RpcMalformedRequest),
                currencyIssue))),
        ammConv),
    field(
        "bridge",
        &Input::bridge,
        withCustomError(type<JsonObject>, rpc::ClioError::RpcMalformedRequest),
        kBridgeJsonValidator,
        bridgeConv),
    field(
        "bridge_account",
        &Input::bridgeAccount,
        withCustomError(accountBase58, rpc::ClioError::RpcMalformedRequest),
        accountId),
    field(
        "xchain_owned_claim_id",
        &Input::xchainOwnedClaimId,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        kBridgeJsonValidator,
        withCustomError(
            ifType<JsonObject>(section(field("xchain_owned_claim_id", required, type<uint32_t>))),
            rpc::ClioError::RpcMalformedRequest),
        xChainClaimIdConv),
    field(
        "xchain_owned_create_account_claim_id",
        &Input::xchainOwnedCreateAccountClaimId,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        kBridgeJsonValidator,
        withCustomError(
            ifType<JsonObject>(
                section(field("xchain_owned_create_account_claim_id", required, type<uint32_t>))),
            rpc::ClioError::RpcMalformedRequest),
        xChainCreateAccountClaimIdConv),
    field(
        "oracle",
        &Input::oracle,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(withCustomError(
            kMalformedRequestHexStringValidator,
            rpc::ClioError::RpcMalformedAddress)),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)),
            // note: Unlike `xrpld`, Clio only supports UInt as input, no string, no
            // `null`, etc.:
            field(
                "oracle_document_id",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(
                    type<uint32_t, std::string>,
                    rpc::ClioError::RpcMalformedOracleDocumentId),
                withCustomError(toNumber, rpc::ClioError::RpcMalformedOracleDocumentId)))),
        oracleConv),
    field(
        "credential",
        &Input::credential,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(withCustomError(
            kMalformedRequestHexStringValidator,
            rpc::ClioError::RpcMalformedAddress)),
        ifType<JsonObject>(section(
            field(
                "subject",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)),
            field(
                "issuer",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)),
            field(
                "credential_type",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<std::string>, rpc::ClioError::RpcMalformedRequest)))),
        credentialConv),
    field(
        "mpt_issuance",
        &Input::mptIssuance,
        withCustomError(uint192Hex, rpc::ClioError::RpcMalformedRequest),
        asUint192),
    field(
        "mptoken",
        &Input::mptoken,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)),
            field(
                "mpt_issuance_id",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(uint192Hex, rpc::ClioError::RpcMalformedRequest)))),
        mptokenConv),
    field(
        "permissioned_domain",
        &Input::permissionedDomain,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)),
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)))),
        permissionedDomainConv),
    field(
        "vault",
        &Input::vault,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)),
            field(
                "owner",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)))),
        vaultConv),
    field(
        "loan_broker",
        &Input::loanBroker,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)),
            field(
                "owner",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedOwner)))),
        loanBrokerConv),
    field(
        "loan",
        &Input::loan,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "loan_seq",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(type<uint32_t>, rpc::ClioError::RpcMalformedRequest)),
            field(
                "loan_broker_id",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(uint256Hex, rpc::ClioError::RpcMalformedRequest)))),
        loanConv),
    field(
        "delegate",
        &Input::delegate,
        withCustomError(type<std::string, JsonObject>, rpc::ClioError::RpcMalformedRequest),
        ifType<std::string>(kMalformedRequestHexStringValidator),
        ifType<JsonObject>(section(
            field(
                "account",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)),
            field(
                "authorize",
                withCustomError(required, rpc::ClioError::RpcMalformedRequest),
                withCustomError(accountBase58, rpc::ClioError::RpcMalformedAddress)))),
        delegateConv),
    field("amendments", &Input::amendments, kMalformedRequestHexStringValidator, asUint256),
    field("fee", &Input::fee, kMalformedRequestHexStringValidator, asUint256),
    field("hashes", &Input::hashes, kMalformedRequestHexStringValidator, asUint256),
    field("nft_offer", &Input::nftOffer, kMalformedRequestHexStringValidator, asUint256),
    field("nunl", &Input::nunl, kMalformedRequestHexStringValidator, asUint256),
    field("signer_list", &Input::signerList, kMalformedRequestHexStringValidator, asUint256),
    field("include_deleted", &Input::includeDeleted, type<bool>, jsonBool));

/** @brief Version-selecting spec (resolved from Input via specFor). */
inline constexpr auto kSpec = versioned<Input>(kInputSpec);

/** @brief ADL hook: resolve the versioned spec from the Input type. */
[[nodiscard]] constexpr auto const&
specFor(Input const*) noexcept
{
    return kSpec;
}

}  // namespace rpc::spec::handlers::ledger_entry
