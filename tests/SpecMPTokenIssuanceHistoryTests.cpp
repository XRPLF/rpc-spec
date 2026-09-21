/**
 * @file
 *  GTest coverage for the Clio-only `mptoken_issuance_history` typed spec.
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/handlers/mptoken_issuance_history/Spec.hpp>
#include <rpcspec/handlers/mptoken_issuance_history/Types.hpp>

#include <xrpl_mock.hpp>

#include <string>

using namespace rpc::spec;

namespace {

constexpr char const* kMptId = "000004C463C52827307480341125DA0577DEFC38405DBADD";
constexpr char const* kAccount = "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh";

auto
parseHistory(std::string const& json)
{
    auto value = boost::json::parse(json);
    return handlers::mptoken_issuance_history::kInputSpec.parse(value);
}

std::string
withMptId(std::string const& extra = "")
{
    return std::string{R"JSON({"mpt_issuance_id": ")JSON"} + kMptId + R"JSON(")JSON" + extra + "}";
}

}  // namespace

TEST(MPTokenIssuanceHistorySpec, MinimalRequestParses)
{
    auto const result = parseHistory(withMptId());
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;

    xrpl::uint192 expected{};
    ASSERT_TRUE(expected.parseHex(kMptId));
    EXPECT_EQ(result->mptIssuanceId, expected);
    EXPECT_FALSE(result->account.has_value());
    EXPECT_FALSE(result->limit.has_value());
    EXPECT_FALSE(static_cast<bool>(result->binary));
    EXPECT_FALSE(static_cast<bool>(result->forward));
}

TEST(MPTokenIssuanceHistorySpec, MissingMptIssuanceIdFails)
{
    auto const result = parseHistory(R"JSON({})JSON");
    EXPECT_FALSE(result.has_value());
}

TEST(MPTokenIssuanceHistorySpec, MalformedMptIssuanceIdFails)
{
    auto const result = parseHistory(R"JSON({"mpt_issuance_id": "NOTAHEX"})JSON");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(MPTokenIssuanceHistorySpec, AccountParses)
{
    auto const result =
        parseHistory(withMptId(std::string{R"JSON(, "account": ")JSON"} + kAccount + "\""));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_TRUE(result->account.has_value());
}

TEST(MPTokenIssuanceHistorySpec, MalformedAccountFails)
{
    auto const result = parseHistory(withMptId(R"JSON(, "account": "not-an-account")JSON"));
    EXPECT_FALSE(result.has_value());
}

TEST(MPTokenIssuanceHistorySpec, TxTypeIsLowercasedAndValidated)
{
    auto const result = parseHistory(withMptId(R"JSON(, "tx_type": "Payment")JSON"));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->transactionTypeInLowercase.has_value());
    EXPECT_EQ(*result->transactionTypeInLowercase, "payment");
}

TEST(MPTokenIssuanceHistorySpec, UnknownTxTypeFails)
{
    auto const result = parseHistory(withMptId(R"JSON(, "tx_type": "NotARealType")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'tx_type'.");
}

TEST(MPTokenIssuanceHistorySpec, LimitIsClampedToMax)
{
    auto const result = parseHistory(withMptId(R"JSON(, "limit": 9999)JSON"));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->limit.has_value());
    EXPECT_EQ(*result->limit, handlers::mptoken_issuance_history::kLimitMax);
}

TEST(MPTokenIssuanceHistorySpec, LimitBelowMinFails)
{
    auto const result = parseHistory(withMptId(R"JSON(, "limit": 0)JSON"));
    EXPECT_FALSE(result.has_value());
}

TEST(MPTokenIssuanceHistorySpec, MarkerParses)
{
    auto const result = parseHistory(withMptId(R"JSON(, "marker": {"ledger": 7, "seq": 9})JSON"));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    ASSERT_TRUE(result->marker.has_value());
    EXPECT_EQ(result->marker->ledger, 7u);
    EXPECT_EQ(result->marker->seq, 9u);
}

TEST(MPTokenIssuanceHistorySpec, NonObjectMarkerIsInvalidMarker)
{
    auto const result = parseHistory(withMptId(R"JSON(, "marker": "nope")JSON"));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "invalidMarker");
}

TEST(MPTokenIssuanceHistorySpec, MarkerMissingSeqFails)
{
    auto const result = parseHistory(withMptId(R"JSON(, "marker": {"ledger": 7})JSON"));
    EXPECT_FALSE(result.has_value());
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
    auto const result = parseHistory(withMptId(R"JSON(, "ledger_index_min": -1)JSON"));
    ASSERT_TRUE(result.has_value()) << "msg: " << result.error().message;
    EXPECT_FALSE(result->ledgerIndexMin.has_value());
}
