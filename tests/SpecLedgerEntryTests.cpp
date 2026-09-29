/**
 * @file
 *  GTest coverage for the `ledger_entry` typed spec.
 *  Compiled under RPCSPEC_IS_XRPLD.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/detail/XrplParse.hpp>
#include <rpcspec/handlers/ledger_entry/Spec.hpp>
#include <rpcspec/handlers/ledger_entry/Types.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <sstream>
#include <string>
#include <variant>

using namespace rpc::spec;
using namespace rpc::spec::handlers::ledger_entry;

namespace {

constexpr auto kAcct1 = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";
constexpr auto kAcct2 = "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK";
constexpr auto kHex64 = "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";
constexpr auto kHex48 = "00000000ABCDEF0123456789ABCDEF0123456789ABCDEF01";

auto
parse(std::string const& json)
{
    auto value = boost::json::parse(json);
    return kInputSpec.parse(value);
}

}  // namespace

TEST(LedgerEntrySpec, check_hex_locator)
{
    auto const result = parse(
        R"JSON({"check": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->check.has_value());

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHex64));
    EXPECT_EQ(*result->check, expected);
}

TEST(LedgerEntrySpec, account_root_locator)
{
    auto const result = parse(R"JSON({"account_root": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->accountRoot.has_value());

    auto const expected = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(*result->accountRoot, *expected);
}

TEST(LedgerEntrySpec, mpt_issuance_hex_locator)
{
    auto const result =
        parse(R"JSON({"mpt_issuance": "00000000ABCDEF0123456789ABCDEF0123456789ABCDEF01"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->mptIssuance.has_value());

    xrpl::uint192 expected;
    ASSERT_TRUE(expected.parseHex(kHex48));
    EXPECT_EQ(*result->mptIssuance, expected);
}

TEST(LedgerEntrySpec, offer_hex_arm)
{
    auto const result = parse(
        R"JSON({"offer": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->offer.has_value());
    EXPECT_TRUE(std::holds_alternative<xrpl::uint256>(*result->offer));

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHex64));
    EXPECT_EQ(std::get<xrpl::uint256>(*result->offer), expected);
}

TEST(LedgerEntrySpec, offer_object_arm)
{
    auto const result =
        parse(R"JSON({"offer": {"account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "seq": 5}})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->offer.has_value());
    ASSERT_TRUE(std::holds_alternative<OfferEntry>(*result->offer));

    auto const& entry = std::get<OfferEntry>(*result->offer);
    auto const expectedAcct = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expectedAcct.has_value());
    EXPECT_EQ(entry.account, *expectedAcct);
    EXPECT_EQ(entry.seq, 5u);
}

TEST(LedgerEntrySpec, directory_object_with_owner_and_sub_index)
{
    auto const result = parse(
        R"JSON({"directory": {"owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sub_index": 42}})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->directory.has_value());
    ASSERT_TRUE(std::holds_alternative<DirectoryEntry>(*result->directory));

    auto const& entry = std::get<DirectoryEntry>(*result->directory);
    ASSERT_TRUE(entry.owner.has_value());
    auto const expectedAcct = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expectedAcct.has_value());
    EXPECT_EQ(*entry.owner, *expectedAcct);
    ASSERT_TRUE(entry.subIndex.has_value());
    EXPECT_EQ(*entry.subIndex, 42u);
    EXPECT_FALSE(entry.dirRoot.has_value());
}

TEST(LedgerEntrySpec, amm_object_arm)
{
    auto const result = parse(R"JSON({
        "amm": {
            "asset":  {"currency": "USD", "issuer": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"},
            "asset2": {"currency": "XRP"}
        }
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->amm.has_value());
    EXPECT_TRUE(std::holds_alternative<AmmEntry>(*result->amm));
}

TEST(LedgerEntrySpec, ripple_state_object_locator)
{
    auto const result = parse(R"JSON({
        "ripple_state": {
            "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"],
            "currency": "USD"
        }
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->rippleStateAccount.has_value());

    auto const expectedAcct1 = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expectedAcct1.has_value());
    EXPECT_EQ(result->rippleStateAccount->accounts[0], *expectedAcct1);

    auto const expectedAcct2 = rpc::spec::detail::accountFromStringStrict(kAcct2);
    ASSERT_TRUE(expectedAcct2.has_value());
    EXPECT_EQ(result->rippleStateAccount->accounts[1], *expectedAcct2);
}

TEST(LedgerEntrySpec, deposit_preauth_authorized_account)
{
    auto const result = parse(R"JSON({
        "deposit_preauth": {
            "owner":      "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "authorized": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"
        }
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->depositPreauth.has_value());
    ASSERT_TRUE(std::holds_alternative<DepositPreauthEntry>(*result->depositPreauth));

    auto const& entry = std::get<DepositPreauthEntry>(*result->depositPreauth);
    ASSERT_TRUE(entry.authorized.has_value());
    auto const expectedAcct = rpc::spec::detail::accountFromStringStrict(kAcct2);
    ASSERT_TRUE(expectedAcct.has_value());
    EXPECT_EQ(*entry.authorized, *expectedAcct);
    EXPECT_FALSE(entry.authorizedCredentials.has_value());
}

TEST(LedgerEntrySpec, deposit_preauth_authorized_credentials)
{
    auto const result = parse(R"JSON({
        "deposit_preauth": {
            "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "authorized_credentials": [
                {"issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK", "credential_type": "ABCD"}
            ]
        }
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->depositPreauth.has_value());
    ASSERT_TRUE(std::holds_alternative<DepositPreauthEntry>(*result->depositPreauth));

    auto const& entry = std::get<DepositPreauthEntry>(*result->depositPreauth);
    ASSERT_FALSE(entry.authorized.has_value());
    ASSERT_TRUE(entry.authorizedCredentials.has_value());
    ASSERT_EQ(entry.authorizedCredentials->size(), 1u);

    auto const& cred = (*entry.authorizedCredentials)[0];
    auto const expectedIssuer = rpc::spec::detail::accountFromStringStrict(kAcct2);
    ASSERT_TRUE(expectedIssuer.has_value());
    EXPECT_EQ(cred.issuer, *expectedIssuer);
    EXPECT_EQ(cred.credentialType, "ABCD");
}

TEST(LedgerEntrySpec, authorized_credentials_issuer_rejects_zero_account)
{
    auto const result = parse(R"JSON({
        "deposit_preauth": {
            "owner": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "authorized_credentials": [
                {"issuer": "rrrrrrrrrrrrrrrrrrrrrhoLvTp", "credential_type": "ABCD"}
            ]
        }
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(LedgerEntrySpec, credential_object_type_accepts_hex)
{
    auto const result = parse(R"JSON({
        "credential": {
            "subject": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "credential_type": "ABCD"
        }
    })JSON");
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
}

TEST(LedgerEntrySpec, credential_object_type_rejects_non_hex)
{
    auto const result = parse(R"JSON({
        "credential": {
            "subject": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "credential_type": "not-hex"
        }
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "credential_type NotHexString");
}

TEST(LedgerEntrySpec, credential_object_type_rejects_empty)
{
    auto const result = parse(R"JSON({
        "credential": {
            "subject": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "issuer": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "credential_type": ""
        }
    })JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "credential_type is empty");
}

TEST(LedgerEntrySpec, bridge_object_locator)
{
    auto const result = parse(R"JSON({
        "bridge": {
            "LockingChainDoor":  "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "IssuingChainDoor":  "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "LockingChainIssue": {"currency": "XRP"},
            "IssuingChainIssue": {"currency": "XRP"}
        }
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->bridge.has_value());

    auto const expectedDoor = rpc::spec::detail::accountFromStringStrict(kAcct1);
    ASSERT_TRUE(expectedDoor.has_value());
    EXPECT_EQ(result->bridge->lockingChainDoor, *expectedDoor);
}

TEST(LedgerEntrySpec, x_chain_owned_claim_id_object_arm)
{
    auto const result = parse(R"JSON({
        "xchain_owned_claim_id": {
            "LockingChainDoor":      "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh",
            "IssuingChainDoor":      "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK",
            "LockingChainIssue":     {"currency": "XRP"},
            "IssuingChainIssue":     {"currency": "XRP"},
            "xchain_owned_claim_id": 7
        }
    })JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->xchainOwnedClaimId.has_value());
    ASSERT_TRUE(std::holds_alternative<XChainClaimIdEntry>(*result->xchainOwnedClaimId));

    auto const& entry = std::get<XChainClaimIdEntry>(*result->xchainOwnedClaimId);
    EXPECT_EQ(entry.claimId, 7u);
}

TEST(LedgerEntrySpec, x_chain_owned_claim_id_hex_arm)
{
    auto const result = parse(
        R"JSON({"xchain_owned_claim_id": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->xchainOwnedClaimId.has_value());
    EXPECT_TRUE(std::holds_alternative<xrpl::uint256>(*result->xchainOwnedClaimId));
}

TEST(LedgerEntrySpec, ledger_index_validated)
{
    auto const result = parse(
        R"JSON({"check": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789", "ledger_index": "validated"})JSON");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ledger.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(result->ledger.value), LedgerShortcut::Validated);
}

TEST(LedgerEntrySpec, ledger_unspecified_when_absent)
{
    auto const result = parse(R"JSON({})JSON");
    if (result.has_value())
    {
        EXPECT_TRUE(result->ledger.isUnspecified());
    }
}

TEST(LedgerEntrySpec, malformed_check_hex_returns_error)
{
    auto const result = parse(R"JSON({"check": "xyz"})JSON");
    EXPECT_FALSE(result.has_value());
}

TEST(LedgerEntrySpec, ripple_state_with_only_one_account_returns_error)
{
    auto const result = parse(R"JSON({
        "ripple_state": {
            "accounts": ["rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"],
            "currency": "USD"
        }
    })JSON");
    EXPECT_FALSE(result.has_value());
}

TEST(LedgerEntrySpec, sponsorship_hex_arm)
{
    auto const result = parse(
        R"JSON({"sponsorship": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->sponsorship.has_value());
    ASSERT_TRUE(std::holds_alternative<xrpl::uint256>(*result->sponsorship));

    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHex64));
    EXPECT_EQ(std::get<xrpl::uint256>(*result->sponsorship), expected);
}

TEST(LedgerEntrySpec, sponsorship_object_arm)
{
    auto const result = parse(
        R"JSON({"sponsorship": {"sponsor": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "sponsee": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"}})JSON");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->sponsorship.has_value());
    ASSERT_TRUE(std::holds_alternative<SponsorshipEntry>(*result->sponsorship));

    auto const& entry = std::get<SponsorshipEntry>(*result->sponsorship);
    auto const sponsor = rpc::spec::detail::accountFromStringStrict(kAcct1);
    auto const sponsee = rpc::spec::detail::accountFromStringStrict(kAcct2);
    ASSERT_TRUE(sponsor.has_value());
    ASSERT_TRUE(sponsee.has_value());
    EXPECT_EQ(entry.sponsor, *sponsor);
    EXPECT_EQ(entry.sponsee, *sponsee);
}

TEST(LedgerEntrySpec, sponsorship_missing_sponsee_returns_error)
{
    auto const result =
        parse(R"JSON({"sponsorship": {"sponsor": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh"}})JSON");
    EXPECT_FALSE(result.has_value());
}

TEST(LedgerEntrySpec, sponsorship_malformed_sponsor_returns_error)
{
    auto const result = parse(
        R"JSON({"sponsorship": {"sponsor": "not-an-account", "sponsee": "rPMh7Pi9ct699iZUTWaytJUoHcJ7cgyziK"}})JSON");
    EXPECT_FALSE(result.has_value());
}

TEST(LedgerEntryDump, all_fields_visible)
{
    std::ostringstream oss;
    rpc::spec::SpecDumpWriter writer{oss};
    rpc::spec::handlers::ledger_entry::kInputSpec.dump(writer);
    auto const text = oss.str();
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
             "sponsorship",
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
        EXPECT_TRUE(text.contains(key)) << "missing from dump: " << key;
}
