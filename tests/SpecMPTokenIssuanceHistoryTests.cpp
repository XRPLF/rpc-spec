/** @file
 *  GTest coverage for the Clio-only `mptoken_issuance_history` typed spec.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/mptoken_issuance_history/Spec.hpp>
#include <rpcspec/handlers/mptoken_issuance_history/Types.hpp>

#include <string>

using namespace rpc::spec;

namespace {

constexpr char const* kMPT_ID = "000004C463C52827307480341125DA0577DEFC38405DBADD";
constexpr char const* kACCOUNT = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

auto
parseHistory(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::mptoken_issuance_history::kInputSpec.parse(value);
}

std::string
withMptId(std::string const& extra = "")
{
    return std::string{R"JSON({"mpt_issuance_id": ")JSON"} + kMPT_ID + R"JSON(")JSON" + extra + "}";
}

}  // namespace

TEST(MPTokenIssuanceHistorySpec, MinimalRequestParses)
{
    auto const r = parseHistory(withMptId());
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;

    xrpl::uint192 expected{};
    ASSERT_TRUE(expected.parseHex(kMPT_ID));
    EXPECT_EQ(r->mptIssuanceId, expected);
    EXPECT_FALSE(r->account.has_value());
    EXPECT_FALSE(r->limit.has_value());
    EXPECT_FALSE(static_cast<bool>(r->binary));
    EXPECT_FALSE(static_cast<bool>(r->forward));
}

TEST(MPTokenIssuanceHistorySpec, MissingMptIssuanceIdFails)
{
    auto const r = parseHistory(R"JSON({})JSON");
    EXPECT_FALSE(r.has_value());
}

TEST(MPTokenIssuanceHistorySpec, MalformedMptIssuanceIdFails)
{
    auto const r = parseHistory(R"JSON({"mpt_issuance_id": "NOTAHEX"})JSON");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(MPTokenIssuanceHistorySpec, AccountParses)
{
    auto const r =
        parseHistory(withMptId(std::string{R"JSON(, "account": ")JSON"} + kACCOUNT + "\""));
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    EXPECT_TRUE(r->account.has_value());
}

TEST(MPTokenIssuanceHistorySpec, MalformedAccountFails)
{
    auto const r = parseHistory(withMptId(R"JSON(, "account": "not-an-account")JSON"));
    EXPECT_FALSE(r.has_value());
}

TEST(MPTokenIssuanceHistorySpec, TxTypeIsLowercasedAndValidated)
{
    auto const r = parseHistory(withMptId(R"JSON(, "tx_type": "Payment")JSON"));
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    ASSERT_TRUE(r->transactionTypeInLowercase.has_value());
    EXPECT_EQ(*r->transactionTypeInLowercase, "payment");
}

TEST(MPTokenIssuanceHistorySpec, UnknownTxTypeFails)
{
    auto const r = parseHistory(withMptId(R"JSON(, "tx_type": "NotARealType")JSON"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "Invalid field 'tx_type'.");
}

TEST(MPTokenIssuanceHistorySpec, LimitIsClampedToMax)
{
    auto const r = parseHistory(withMptId(R"JSON(, "limit": 9999)JSON"));
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    ASSERT_TRUE(r->limit.has_value());
    EXPECT_EQ(*r->limit, handlers::mptoken_issuance_history::kLimitMax);
}

TEST(MPTokenIssuanceHistorySpec, LimitBelowMinFails)
{
    auto const r = parseHistory(withMptId(R"JSON(, "limit": 0)JSON"));
    EXPECT_FALSE(r.has_value());
}

TEST(MPTokenIssuanceHistorySpec, MarkerParses)
{
    auto const r = parseHistory(withMptId(R"JSON(, "marker": {"ledger": 7, "seq": 9})JSON"));
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    ASSERT_TRUE(r->marker.has_value());
    EXPECT_EQ(r->marker->ledger, 7u);
    EXPECT_EQ(r->marker->seq, 9u);
}

TEST(MPTokenIssuanceHistorySpec, NonObjectMarkerIsInvalidMarker)
{
    auto const r = parseHistory(withMptId(R"JSON(, "marker": "nope")JSON"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r.error().message, "invalidMarker");
}

TEST(MPTokenIssuanceHistorySpec, MarkerMissingSeqFails)
{
    auto const r = parseHistory(withMptId(R"JSON(, "marker": {"ledger": 7})JSON"));
    EXPECT_FALSE(r.has_value());
}

TEST(MPTokenIssuanceHistorySpec, BinaryAndForwardAreStrictBools)
{
    auto const ok = parseHistory(withMptId(R"JSON(, "binary": true, "forward": true)JSON"));
    ASSERT_TRUE(ok.has_value()) << "msg: " << ok.error().message;
    EXPECT_TRUE(static_cast<bool>(ok->binary));
    EXPECT_TRUE(static_cast<bool>(ok->forward));

    auto const bad = parseHistory(withMptId(R"JSON(, "binary": 1)JSON"));
    EXPECT_FALSE(bad.has_value());
}

TEST(MPTokenIssuanceHistorySpec, LedgerIndexMinusOneSentinelIsUnset)
{
    auto const r = parseHistory(withMptId(R"JSON(, "ledger_index_min": -1)JSON"));
    ASSERT_TRUE(r.has_value()) << "msg: " << r.error().message;
    EXPECT_FALSE(r->ledgerIndexMin.has_value());
}
