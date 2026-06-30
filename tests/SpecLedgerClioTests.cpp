#include <rpcspec/Ledger.hpp>
#include <rpcspec/Typed.hpp>

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>

#include <variant>

using namespace rpc::spec;

namespace {
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

TEST(LedgerSpecifierClio, DefaultIsValidated)
{
    static_assert(kDefaultLedgerShortcut == LedgerShortcut::Validated);
    LedgerSpecifier const def{};
    ASSERT_TRUE(def.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(def.value), LedgerShortcut::Validated);
}

TEST(LedgerSelectorClio, NeitherFieldYieldsValidated)
{
    auto const led = parseLedger(R"JSON({})JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Validated);
}

TEST(LedgerSelectorClio, EmptyIndexStringYieldsValidated)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Validated);
}

TEST(LedgerSelectorClio, ExplicitShortcutsArePreserved)
{
    EXPECT_EQ(std::get<LedgerShortcut>(parseLedger(R"JSON({ "ledger_index": "current" })JSON").value), LedgerShortcut::Current);
    EXPECT_EQ(std::get<LedgerShortcut>(parseLedger(R"JSON({ "ledger_index": "closed" })JSON").value), LedgerShortcut::Closed);
    EXPECT_EQ(std::get<LedgerShortcut>(parseLedger(R"JSON({ "ledger_index": "validated" })JSON").value), LedgerShortcut::Validated);
}
