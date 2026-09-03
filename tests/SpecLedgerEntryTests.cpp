/** @file
 *  GTest coverage for the `ledger_entry` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/ledger_entry/Spec.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>

#include <xrpl_mock.hpp>

#include <sstream>
#include <string>
#include <variant>

using namespace rpc::spec;
using namespace rpc::spec::handlers::ledger_entry;

namespace {

constexpr char const* kACCT1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr char const* kACCT2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr char const* kHEX64 = "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";
constexpr char const* kHEX48 = "00000000ABCDEF0123456789ABCDEF0123456789ABCDEF01";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(LedgerEntrySpec, CheckHexLocator)
{
    auto const r = parse(
        R"JSON({"check": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->check.has_value());

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHEX64));
    EXPECT_EQ(*r->check, expected);
}

TEST(LedgerEntrySpec, AccountRootLocator)
{
    auto const r = parse(R"JSON({"account_root": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->accountRoot.has_value());

    auto const expected = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(*r->accountRoot, *expected);
}

TEST(LedgerEntrySpec, MptIssuanceHexLocator)
{
    auto const r =
        parse(R"JSON({"mpt_issuance": "00000000ABCDEF0123456789ABCDEF0123456789ABCDEF01"})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->mptIssuance.has_value());

    xrpl::uint192 expected;
    ASSERT_TRUE(expected.parseHex(kHEX48));
    EXPECT_EQ(*r->mptIssuance, expected);
}

TEST(LedgerEntrySpec, OfferHexArm)
{
    auto const r = parse(
        R"JSON({"offer": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->offer.has_value());
    EXPECT_TRUE(std::holds_alternative<xrpl::uint256>(*r->offer));

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHEX64));
    EXPECT_EQ(std::get<xrpl::uint256>(*r->offer), expected);
}

TEST(LedgerEntrySpec, OfferObjectArm)
{
    auto const r =
        parse(R"JSON({"offer": {"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "seq": 5}})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->offer.has_value());
    ASSERT_TRUE(std::holds_alternative<OfferEntry>(*r->offer));

    auto const& entry = std::get<OfferEntry>(*r->offer);
    auto const expectedAcct = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expectedAcct.has_value());
    EXPECT_EQ(entry.account, *expectedAcct);
    EXPECT_EQ(entry.seq, 5u);
}

TEST(LedgerEntrySpec, DirectoryObjectWithOwnerAndSubIndex)
{
    auto const r = parse(
        R"JSON({"directory": {"owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sub_index": 42}})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->directory.has_value());
    ASSERT_TRUE(std::holds_alternative<DirectoryEntry>(*r->directory));

    auto const& entry = std::get<DirectoryEntry>(*r->directory);
    ASSERT_TRUE(entry.owner.has_value());
    auto const expectedAcct = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expectedAcct.has_value());
    EXPECT_EQ(*entry.owner, *expectedAcct);
    ASSERT_TRUE(entry.subIndex.has_value());
    EXPECT_EQ(*entry.subIndex, 42u);
    EXPECT_FALSE(entry.dirRoot.has_value());
}

TEST(LedgerEntrySpec, AmmObjectArm)
{
    auto const r = parse(R"JSON({
        "amm": {
            "asset":  {"currency": "USD", "issuer": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"},
            "asset2": {"currency": "XRP"}
        }
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->amm.has_value());
    EXPECT_TRUE(std::holds_alternative<AmmEntry>(*r->amm));
}

TEST(LedgerEntrySpec, RippleStateObjectLocator)
{
    auto const r = parse(R"JSON({
        "ripple_state": {
            "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"],
            "currency": "USD"
        }
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->rippleStateAccount.has_value());

    auto const expectedAcct1 = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expectedAcct1.has_value());
    EXPECT_EQ(r->rippleStateAccount->accounts[0], *expectedAcct1);

    auto const expectedAcct2 = rpc::spec::detail::accountFromStringStrict(kACCT2);
    ASSERT_TRUE(expectedAcct2.has_value());
    EXPECT_EQ(r->rippleStateAccount->accounts[1], *expectedAcct2);
}

TEST(LedgerEntrySpec, DepositPreauthAuthorizedAccount)
{
    auto const r = parse(R"JSON({
        "deposit_preauth": {
            "owner":      "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "authorized": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"
        }
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->depositPreauth.has_value());
    ASSERT_TRUE(std::holds_alternative<DepositPreauthEntry>(*r->depositPreauth));

    auto const& entry = std::get<DepositPreauthEntry>(*r->depositPreauth);
    ASSERT_TRUE(entry.authorized.has_value());
    auto const expectedAcct = rpc::spec::detail::accountFromStringStrict(kACCT2);
    ASSERT_TRUE(expectedAcct.has_value());
    EXPECT_EQ(*entry.authorized, *expectedAcct);
    EXPECT_FALSE(entry.authorizedCredentials.has_value());
}

TEST(LedgerEntrySpec, DepositPreauthAuthorizedCredentials)
{
    auto const r = parse(R"JSON({
        "deposit_preauth": {
            "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "authorized_credentials": [
                {"issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK", "credential_type": "ABCD"}
            ]
        }
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->depositPreauth.has_value());
    ASSERT_TRUE(std::holds_alternative<DepositPreauthEntry>(*r->depositPreauth));

    auto const& entry = std::get<DepositPreauthEntry>(*r->depositPreauth);
    ASSERT_FALSE(entry.authorized.has_value());
    ASSERT_TRUE(entry.authorizedCredentials.has_value());
    ASSERT_EQ(entry.authorizedCredentials->size(), 1u);

    auto const& cred = (*entry.authorizedCredentials)[0];
    auto const expectedIssuer = rpc::spec::detail::accountFromStringStrict(kACCT2);
    ASSERT_TRUE(expectedIssuer.has_value());
    EXPECT_EQ(cred.issuer, *expectedIssuer);
    EXPECT_EQ(cred.credentialType, "ABCD");
}

TEST(LedgerEntrySpec, AuthorizedCredentialsIssuerRejectsZeroAccount)
{
    auto const r = parse(R"JSON({
        "deposit_preauth": {
            "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "authorized_credentials": [
                {"issuer": "rrrrrrrrrrrrrrrrrrrrrhoLvTp", "credential_type": "ABCD"}
            ]
        }
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerEntrySpec, CredentialObjectTypeAcceptsHex)
{
    auto const r = parse(R"JSON({
        "credential": {
            "subject": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "credential_type": "ABCD"
        }
    })JSON");
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
}

TEST(LedgerEntrySpec, CredentialObjectTypeRejectsNonHex)
{
    auto const r = parse(R"JSON({
        "credential": {
            "subject": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "credential_type": "not-hex"
        }
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "credential_type NotHexString");
}

TEST(LedgerEntrySpec, CredentialObjectTypeRejectsEmpty)
{
    auto const r = parse(R"JSON({
        "credential": {
            "subject": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "credential_type": ""
        }
    })JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "credential_type is empty");
}

TEST(LedgerEntrySpec, BridgeObjectLocator)
{
    auto const r = parse(R"JSON({
        "bridge": {
            "LockingChainDoor":  "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "IssuingChainDoor":  "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "LockingChainIssue": {"currency": "XRP"},
            "IssuingChainIssue": {"currency": "XRP"}
        }
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->bridge.has_value());

    auto const expectedDoor = rpc::spec::detail::accountFromStringStrict(kACCT1);
    ASSERT_TRUE(expectedDoor.has_value());
    EXPECT_EQ(r->bridge->lockingChainDoor, *expectedDoor);
}

TEST(LedgerEntrySpec, XChainOwnedClaimIdObjectArm)
{
    auto const r = parse(R"JSON({
        "xchain_owned_claim_id": {
            "LockingChainDoor":      "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "IssuingChainDoor":      "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "LockingChainIssue":     {"currency": "XRP"},
            "IssuingChainIssue":     {"currency": "XRP"},
            "xchain_owned_claim_id": 7
        }
    })JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->xchainOwnedClaimId.has_value());
    ASSERT_TRUE(std::holds_alternative<XChainClaimIdEntry>(*r->xchainOwnedClaimId));

    auto const& entry = std::get<XChainClaimIdEntry>(*r->xchainOwnedClaimId);
    EXPECT_EQ(entry.claimId, 7u);
}

TEST(LedgerEntrySpec, XChainOwnedClaimIdHexArm)
{
    auto const r = parse(
        R"JSON({"xchain_owned_claim_id": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->xchainOwnedClaimId.has_value());
    EXPECT_TRUE(std::holds_alternative<xrpl::uint256>(*r->xchainOwnedClaimId));
}

TEST(LedgerEntrySpec, LedgerIndexValidated)
{
    auto const r = parse(
        R"JSON({"check": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789", "ledger_index": "validated"})JSON");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ledger.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(r->ledger.value), LedgerShortcut::Validated);
}

TEST(LedgerEntrySpec, LedgerUnspecifiedWhenAbsent)
{
    auto const r = parse(R"JSON({})JSON");
    if (r.has_value())
    {
        EXPECT_TRUE(r->ledger.isUnspecified());
    }
}

TEST(LedgerEntrySpec, MalformedCheckHexReturnsError)
{
    auto const r = parse(R"JSON({"check": "xyz"})JSON");
    EXPECT_FALSE(r.has_value());
}

TEST(LedgerEntrySpec, RippleStateWithOnlyOneAccountReturnsError)
{
    auto const r = parse(R"JSON({
        "ripple_state": {
            "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"],
            "currency": "USD"
        }
    })JSON");
    EXPECT_FALSE(r.has_value());
}

TEST(LedgerEntryDump, AllFieldsVisible)
{
    std::ostringstream oss;
    rpc::spec::SpecDumpWriter w{oss};
    rpc::spec::handlers::ledger_entry::kInputSpec.dump(w);
    auto const s = oss.str();
    for (auto const* key : {
             "ledger_hash",
             "ledger_index",
             "index",
             "account_root",
             "did",
             "mpt_issuance",
             "directory",
             "offer",
             "ripple_state",
             "escrow",
             "deposit_preauth",
             "ticket",
             "amm",
             "mptoken",
             "permissioned_domain",
             "vault",
             "loan_broker",
             "loan",
             "oracle",
             "credential",
             "delegate",
             "bridge",
             "bridge_account",
             "xchain_owned_claim_id",
             "xchain_owned_create_account_claim_id",
             "check",
             "payment_channel",
             "nft_page",
             "nft_offer",
             "signer_list",
             "amendments",
             "fee",
             "hashes",
             "nunl",
             "binary",
             "include_deleted",
         })
        EXPECT_NE(s.find(key), std::string::npos) << "missing from dump: " << key;
}
