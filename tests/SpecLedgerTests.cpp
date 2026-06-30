#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <variant>

using namespace rpc::spec;

namespace {

constexpr char const* kHASH64 = "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";

struct LedgerOnlyInput {
    LedgerSpecifier ledger;
};

constexpr auto kLEDGER_SPEC = spec<LedgerOnlyInput>(ledgerSelector(&LedgerOnlyInput::ledger));

LedgerSpecifier
parseLedger(char const* json)
{
    auto value = boost::json::parse(json);
    auto const r = kLEDGER_SPEC.parse(value);
    EXPECT_TRUE(r.has_value());
    return r->ledger;
}

}  // namespace

TEST(LedgerSpecifier, DefaultConstructsToServerDefault)
{
    LedgerSpecifier const def{};
    EXPECT_TRUE(def.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(def.value), kDefaultLedgerShortcut);
    EXPECT_EQ(std::get<LedgerShortcut>(def.value), LedgerShortcut::Current);  // rippled build
}

TEST(LedgerSelector, NeitherFieldYieldsServerDefault)
{
    auto const led = parseLedger(R"JSON({})JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Current);
}

TEST(LedgerSelector, EmptyIndexStringYieldsServerDefault)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), kDefaultLedgerShortcut);
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
    auto const r = kLEDGER_SPEC.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, OutOfRangeNumericIndexFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": 9999999999 })JSON");
    auto const r = kLEDGER_SPEC.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, NonStringNonNumberIndexFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": true })JSON");
    auto const r = kLEDGER_SPEC.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, ValidHashYieldsHash)
{
    auto const led = parseLedger(R"JSON({ "ledger_hash": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789" })JSON");
    ASSERT_TRUE(led.isHash());
    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHASH64));
    EXPECT_EQ(std::get<xrpl::uint256>(led.value), expected);
}

TEST(LedgerSelector, MalformedHashFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_hash": "DEADBEEF" })JSON");
    auto const r = kLEDGER_SPEC.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, NonStringHashFails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_hash": 123 })JSON");
    auto const r = kLEDGER_SPEC.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(LedgerSelector, BothHashAndIndexFails)
{
    auto value = boost::json::parse(
        R"JSON({ "ledger_hash": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789", "ledger_index": 5 })JSON"
    );
    auto const r = kLEDGER_SPEC.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcInvalidParams);
}

namespace {
struct AccountAndLedgerInput {
    xrpl::AccountID account;
    LedgerSpecifier ledger;
};

constexpr auto kACCT_LEDGER_SPEC = spec<AccountAndLedgerInput>(
    field("account", &AccountAndLedgerInput::account, required, accountId),
    ledgerSelector(&AccountAndLedgerInput::ledger)
);
}  // namespace

TEST(LedgerSelector, ComposesAlongsideOtherFields)
{
    auto value = boost::json::parse(R"JSON({ "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "ledger_index": "validated" })JSON");
    auto const r = kACCT_LEDGER_SPEC.parse(value);
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->ledger.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(r->ledger.value), LedgerShortcut::Validated);
}

TEST(LedgerSelector, ComposedSpecDefaultsLedgerWhenAbsent)
{
    auto value = boost::json::parse(R"JSON({ "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh" })JSON");
    auto const r = kACCT_LEDGER_SPEC.parse(value);
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->ledger.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(r->ledger.value), LedgerShortcut::Current);
}

TEST(LedgerSelector, SpecIsConstantEvaluable)
{
    // Forces consteval construction (incl. the boost::pfr completeness guard).
    static constexpr auto kSpec = spec<LedgerOnlyInput>(ledgerSelector(&LedgerOnlyInput::ledger));
    (void)kSpec;
    SUCCEED();
}
