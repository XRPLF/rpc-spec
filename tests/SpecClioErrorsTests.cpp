/**
 * @file
 * @brief Clio-backend tests pinning Clio's RPC error codes and messages.
 *
 * These messages live behind RPCSPEC_IS_CLIO in Errors.hpp and the validators that use them,
 * which means this translation unit - compiled with RPCSPEC_IS_CLIO=1, see rpcspec_clio_tests -
 * is the only place that branch can be exercised. The complementary xrpld wording is covered by
 * the SpecValidatorTests / SpecLedgerTests suites.
 *
 * @see notStringFieldMessage, malformedFieldMessage, malformedLedgerIndexMessage,
 *      malformedCursorMessage
 */

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>

#include <string_view>

using namespace rpc::spec;

namespace {

/**
 * @brief Runs @p json through @p spec and returns the resulting Status.
 */
template <typename Spec>
rpc::Status
statusOf(Spec const& spec, std::string_view json)
{
    auto req = boost::json::parse(json);
    auto const r = spec.process(req);
    EXPECT_FALSE(r.has_value()) << json;
    return r.has_value() ? rpc::Status{} : r.error();
}

}  // namespace

// ---------------------------------------------------------------------------
// The <field>NotString / <field>Malformed pair, which xrpld collapses into one
// invalidFieldMessage.
// ---------------------------------------------------------------------------

TEST(ClioErrors, HexFieldMessagesAreBuiltFromTheKey)
{
    EXPECT_EQ(rpc::notStringFieldMessage("nft_id"), "nft_idNotString");
    EXPECT_EQ(rpc::malformedFieldMessage("nft_id"), "nft_idMalformed");
}

TEST(ClioErrors, Uint256ValidatorReportsNotStringThenMalformed)
{
    static constexpr auto kSpec = RpcSpec{field("nft_id", uint256Hex)};

    auto const notString = statusOf(kSpec, R"JSON({"nft_id": 1})JSON");
    EXPECT_EQ(notString, rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(notString.message, "nft_idNotString");

    auto const malformed = statusOf(kSpec, R"JSON({"nft_id": "xxx"})JSON");
    EXPECT_EQ(malformed, rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(malformed.message, "nft_idMalformed");
}

TEST(ClioErrors, Uint192ValidatorReportsNotStringThenMalformed)
{
    static constexpr auto kSpec = RpcSpec{field("mpt_issuance_id", uint192Hex)};

    EXPECT_EQ(
        statusOf(kSpec, R"JSON({"mpt_issuance_id": true})JSON").message,
        "mpt_issuance_idNotString");
    EXPECT_EQ(
        statusOf(kSpec, R"JSON({"mpt_issuance_id": "nothex"})JSON").message,
        "mpt_issuance_idMalformed");
}

// ---------------------------------------------------------------------------
// ledger_hash / ledger_index, reached through ledgerSelector by 28 methods.
// ---------------------------------------------------------------------------

namespace {

struct LedgerOnlyInput
{
    LedgerSpecifier ledger;
};

constexpr auto kLedgerSpec = spec<LedgerOnlyInput>(ledgerSelector(&LedgerOnlyInput::ledger));

}  // namespace

TEST(ClioErrors, LedgerHashReportsNotStringThenMalformed)
{
    auto notString = boost::json::parse(R"JSON({"ledger_hash": 1})JSON");
    auto const r1 = kLedgerSpec.parse(notString);
    ASSERT_FALSE(r1.has_value());
    EXPECT_EQ(r1.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(r1.error().message, "ledger_hashNotString");

    auto malformed = boost::json::parse(R"JSON({"ledger_hash": "xxx"})JSON");
    auto const r2 = kLedgerSpec.parse(malformed);
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r2.error().message, "ledger_hashMalformed");
}

TEST(ClioErrors, LedgerIndexUsesOneTokenForEveryFailure)
{
    for (auto const* json :
         {R"JSON({"ledger_index": true})JSON",
          R"JSON({"ledger_index": "notanumber"})JSON",
          R"JSON({"ledger_index": -1})JSON"})
    {
        auto req = boost::json::parse(json);
        auto const r = kLedgerSpec.parse(req);
        ASSERT_FALSE(r.has_value()) << json;
        EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams) << json;
        EXPECT_EQ(r.error().message, "ledgerIndexMalformed") << json;
    }
}

TEST(ClioErrors, LedgerIndexAcceptsValidatedAndSequences)
{
    for (auto const* json :
         {R"JSON({"ledger_index": "validated"})JSON",
          R"JSON({"ledger_index": 42})JSON",
          R"JSON({"ledger_index": "42"})JSON"})
    {
        auto req = boost::json::parse(json);
        EXPECT_TRUE(kLedgerSpec.parse(req).has_value()) << json;
    }
}

TEST(ClioErrors, LedgerIndexRejectsCurrentAndClosed)
{
    // Clio holds neither. Requests naming them are forwarded to xrpld before validation, except
    // for Clio-only methods, which xrpld cannot answer - so the spec rejects them rather than
    // hand a shortcut to a resolver that only understands `validated`.
    for (auto const* json :
         {R"JSON({"ledger_index": "current"})JSON", R"JSON({"ledger_index": "closed"})JSON"})
    {
        auto req = boost::json::parse(json);
        auto const r = kLedgerSpec.parse(req);
        ASSERT_FALSE(r.has_value()) << json;
        EXPECT_EQ(r.error().message, "ledgerIndexMalformed") << json;
    }
}

TEST(ClioErrors, LedgerIndexValidatorMatchesTheSelector)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};

    EXPECT_EQ(statusOf(kSpec, R"JSON({"ledger_index": true})JSON").message, "ledgerIndexMalformed");
    EXPECT_EQ(
        statusOf(kSpec, R"JSON({"ledger_index": "current"})JSON").message, "ledgerIndexMalformed");
    EXPECT_EQ(
        statusOf(kSpec, R"JSON({"ledger_index": "closed"})JSON").message, "ledgerIndexMalformed");
}

// ---------------------------------------------------------------------------
// The account cursor, whose parse failure names no field.
// ---------------------------------------------------------------------------

TEST(ClioErrors, AccountMarkerReportsNotStringThenMalformedCursor)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};

    EXPECT_EQ(statusOf(kSpec, R"JSON({"marker": 1})JSON").message, "markerNotString");
    EXPECT_EQ(statusOf(kSpec, R"JSON({"marker": "nocomma"})JSON").message, "Malformed cursor.");
    EXPECT_EQ(statusOf(kSpec, R"JSON({"marker": "xxx,1"})JSON").message, "Malformed cursor.");
    EXPECT_EQ(rpc::malformedCursorMessage("marker"), "Malformed cursor.");
}

TEST(ClioErrors, LedgerHashTakesPrecedenceOverLedgerIndexOnError)
{
    // Every handler declared ledger_hash ahead of ledger_index, so with both malformed the
    // hash error is the one reported.
    auto req = boost::json::parse(R"JSON({"ledger_hash": "xx", "ledger_index": "yy"})JSON");
    auto const r = kLedgerSpec.parse(req);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().message, "ledger_hashMalformed");
}

TEST(ClioErrors, LedgerIndexStillValidatedWhenHashIsValid)
{
    auto req = boost::json::parse(
        R"JSON({"ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": "yy"})JSON");
    auto const r = kLedgerSpec.parse(req);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().message, "ledgerIndexMalformed");
}

TEST(ClioErrors, ToNumberRejectsValuesThatWouldTruncateToUint32)
{
    static constexpr auto kSpec = RpcSpec{field("oracle_document_id", toNumber)};

    // Accepted and readable as a uint32.
    auto ok = boost::json::parse(R"JSON({"oracle_document_id": "4294967295"})JSON");
    EXPECT_TRUE(kSpec.process(ok).has_value());

    // One past uint32: must be rejected rather than silently wrapped.
    for (auto const* json :
         {R"JSON({"oracle_document_id": "4294967296"})JSON",
          R"JSON({"oracle_document_id": "99999999999"})JSON",
          R"JSON({"oracle_document_id": "-1"})JSON"})
    {
        auto req = boost::json::parse(json);
        EXPECT_FALSE(kSpec.process(req).has_value()) << json;
    }
}
