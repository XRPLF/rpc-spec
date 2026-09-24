#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>

#include <Backend.hpp>  // IWYU pragma: keep
#include <xrpl_mock.hpp>

#include <cstdint>

using namespace rpc::spec;

namespace {

constexpr auto kHash64 = "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";

struct LedgerOnlyInput
{
    LedgerSpecifier ledger;
};

constexpr auto kLedgerSpec = spec<LedgerOnlyInput>(ledgerSelector(&LedgerOnlyInput::ledger));

LedgerSpecifier
parseLedger(char const* json)
{
    auto value = boost::json::parse(json);
    auto const result = kLedgerSpec.parse(value);
    EXPECT_TRUE(result.has_value());
    return result->ledger;
}

}  // namespace

TEST(LedgerSpecifier, DefaultConstructsToUnspecified)
{
    LedgerSpecifier const def{};
    EXPECT_TRUE(def.isUnspecified());
    EXPECT_FALSE(def.isShortcut());
}

TEST(LedgerSpecifier, ResolvedAppliesServerDefault)
{
    auto const result = LedgerSpecifier{}.resolved();
    ASSERT_TRUE(result.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(result.value), kDefaultLedgerShortcut);
    EXPECT_EQ(std::get<LedgerShortcut>(result.value), LedgerShortcut::Current);  // xrpld build
}

TEST(LedgerSpecifier, ResolvedLeavesConcreteValueUnchanged)
{
    LedgerSpecifier const seq{uint32_t{42}};
    EXPECT_EQ(seq.resolved(), seq);
}

TEST(LedgerSelector, NeitherFieldYieldsUnspecified)
{
    auto const led = parseLedger(R"JSON({})JSON");
    EXPECT_TRUE(led.isUnspecified());
    // The server default is applied only on resolution.
    EXPECT_EQ(std::get<LedgerShortcut>(led.resolved().value), LedgerShortcut::Current);
}

TEST(LedgerSelector, EmptyIndexStringFails)
{
    // An empty ledger_index is malformed. Only an ABSENT ledger_index means "use the
    // server default" — see NeitherFieldYieldsUnspecified above.
    auto value = boost::json::parse(R"JSON({ "ledger_index": "" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, TrailingGarbageIndexStringFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": "30abc" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, ShortcutValidated)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "validated" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Validated);
}

TEST(LedgerSelector, ShortcutCurrent)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "current" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Current);
}

TEST(LedgerSelector, ShortcutClosed)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "closed" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Closed);
}

TEST(LedgerSelector, NumericIndexYieldsSequence)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": 12345 })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 12345u);
}

TEST(LedgerSelector, NumericStringIndexYieldsSequence)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "67890" })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 67890u);
}

TEST(LedgerSelector, MaxUint32IndexIsAccepted)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": 4294967295 })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 4294967295u);
}

TEST(LedgerSelector, UnknownIndexStringFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": "latest" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, OutOfRangeNumericIndexFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": 9999999999 })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, NonStringNonNumberIndexFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": true })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, ValidHashYieldsHash)
{
    auto const led = parseLedger(
        R"JSON({ "ledger_hash": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789" })JSON");
    ASSERT_TRUE(led.isHash());
    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHash64));
    EXPECT_EQ(std::get<xrpl::uint256>(led.value), expected);
}

TEST(LedgerSelector, MalformedHashFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_hash": "DEADBEEF" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, NonStringHashFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_hash": 123 })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

// Not mutually exclusive: when both are present, ledger_hash wins (mirrors the
// historical getLedgerHeaderFromHashOrSeq contract). This must NOT error.
TEST(LedgerSelector, BothHashAndIndexPrefersHash)
{
    auto const led = parseLedger(
        R"JSON({ "ledger_hash": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789", "ledger_index": 5 })JSON");
    ASSERT_TRUE(led.isHash());
    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHash64));
    EXPECT_EQ(std::get<xrpl::uint256>(led.value), expected);
}

TEST(LedgerSelector, BothHashAndMalformedIndexFails)
{
    // ledger_hash wins when both are valid, but a malformed ledger_index must still be
    // reported rather than skipped just because a usable hash accompanied it.
    auto value = boost::json::parse(
        R"JSON({ "ledger_hash": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789", "ledger_index": "nonsense" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

namespace {
struct AccountAndLedgerInput
{
    xrpl::AccountID account;
    LedgerSpecifier ledger;
};

constexpr auto kAcctLedgerSpec = spec<AccountAndLedgerInput>(
    field("account", &AccountAndLedgerInput::account, required, accountId),
    ledgerSelector(&AccountAndLedgerInput::ledger));
}  // namespace

TEST(LedgerSelector, ComposesAlongsideOtherFields)
{
    auto value = boost::json::parse(
        R"JSON({ "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "ledger_index": "validated" })JSON");
    auto const result = kAcctLedgerSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->ledger.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(result->ledger.value), LedgerShortcut::Validated);
}

TEST(LedgerSelector, ComposedSpecLeavesLedgerUnspecifiedWhenAbsent)
{
    auto value =
        boost::json::parse(R"JSON({ "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh" })JSON");
    auto const result = kAcctLedgerSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ledger.isUnspecified());
}

TEST(LedgerSelector, SpecIsConstantEvaluable)
{
    // Forces consteval construction (incl. the boost::pfr completeness guard).
    static constexpr auto kSpec = spec<LedgerOnlyInput>(ledgerSelector(&LedgerOnlyInput::ledger));
    (void)kSpec;
    SUCCEED();
}
