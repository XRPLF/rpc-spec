#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/Ledger.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Validators.hpp>
#include <rpcspec/handlers/ledger/Spec.hpp>

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

TEST(LedgerSpecifier, default_constructs_to_unspecified)
{
    LedgerSpecifier const def{};
    EXPECT_TRUE(def.isUnspecified());
    EXPECT_FALSE(def.isShortcut());
}

TEST(LedgerSpecifier, resolved_applies_server_default)
{
    auto const result = LedgerSpecifier{}.resolved();
    ASSERT_TRUE(result.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(result.value), kDefaultLedgerShortcut);
    EXPECT_EQ(std::get<LedgerShortcut>(result.value), LedgerShortcut::Current);  // xrpld build
}

TEST(LedgerSpecifier, resolved_leaves_concrete_value_unchanged)
{
    LedgerSpecifier const seq{uint32_t{42}};
    EXPECT_EQ(seq.resolved(), seq);
}

TEST(LedgerSelector, neither_field_yields_unspecified)
{
    auto const led = parseLedger(R"JSON({})JSON");
    EXPECT_TRUE(led.isUnspecified());
    // The server default is applied only on resolution.
    EXPECT_EQ(std::get<LedgerShortcut>(led.resolved().value), LedgerShortcut::Current);
}

TEST(LedgerSelector, empty_index_string_yields_current)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Current);
}

TEST(LedgerSelector, plus_signed_index_string_yields_sequence)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "+42" })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 42u);
}

TEST(LedgerSelector, trailing_garbage_index_string_fails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": "30abc" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(LedgerSelector, shortcut_validated)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "validated" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Validated);
}

TEST(LedgerSelector, shortcut_current)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "current" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Current);
}

TEST(LedgerSelector, shortcut_closed)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "closed" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Closed);
}

TEST(LedgerSelector, numeric_index_yields_sequence)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": 12345 })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 12345u);
}

TEST(LedgerSelector, numeric_string_index_yields_sequence)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": "67890" })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 67890u);
}

TEST(LedgerSelector, max_uint32_index_is_accepted)
{
    auto const led = parseLedger(R"JSON({ "ledger_index": 4294967295 })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 4294967295u);
}

TEST(LedgerSelector, unknown_index_string_fails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": "latest" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'ledger_index', not string or number.");
}

TEST(LedgerSelector, negative_index_fails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": -1 })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'ledger_index', not string or number.");
}

TEST(LedgerSelector, out_of_range_numeric_index_fails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_index": 9999999999 })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'ledger_index', not string or number.");
}

TEST(LedgerSelector, non_string_non_number_index_fails)
{
    for (auto const* json :
         {R"JSON({ "ledger_index": true })JSON",
          R"JSON({ "ledger_index": 1.5 })JSON",
          R"JSON({ "ledger_index": null })JSON"})
    {
        auto value = boost::json::parse(json);
        auto const result = kLedgerSpec.parse(value);
        ASSERT_FALSE(result.has_value()) << json;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << json;
        EXPECT_EQ(result.error().message, "Invalid field 'ledger_index', not string or number.")
            << json;
    }
}

TEST(LedgerSelector, valid_hash_yields_hash)
{
    auto const led = parseLedger(
        R"JSON({ "ledger_hash": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789" })JSON");
    ASSERT_TRUE(led.isHash());
    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHash64));
    EXPECT_EQ(std::get<xrpl::uint256>(led.value), expected);
}

TEST(LedgerSelector, malformed_hash_fails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_hash": "DEADBEEF" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'ledger_hash', not hex string.");
}

TEST(LedgerSelector, non_string_hash_fails)
{
    auto value = boost::json::parse(R"JSON({ "ledger_hash": 123 })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'ledger_hash', not hex string.");
}

TEST(LedgerSelector, both_hash_and_index_fail)
{
    auto value = boost::json::parse(
        R"JSON({ "ledger_hash": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789", "ledger_index": 5 })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(
        result.error().message, "Exactly one of 'ledger_hash' or 'ledger_index' can be specified.");
}

TEST(LedgerSelector, field_count_is_checked_before_values)
{
    auto value = boost::json::parse(R"JSON({ "ledger_hash": 1, "ledger_index": "nonsense" })JSON");
    auto const result = kLedgerSpec.parse(value);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(
        result.error().message, "Exactly one of 'ledger_hash' or 'ledger_index' can be specified.");
}

TEST(LedgerSelector, legacy_ledger_with_another_selector_fails)
{
    for (auto const* json :
         {R"JSON({ "ledger": 5, "ledger_index": 5 })JSON",
          R"JSON({ "ledger": 5, "ledger_hash": "DEADBEEF" })JSON"})
    {
        auto value = boost::json::parse(json);
        auto const result = kLedgerSpec.parse(value);
        ASSERT_FALSE(result.has_value()) << json;
        EXPECT_EQ(
            result.error().message,
            "Exactly one of 'ledger', 'ledger_hash', or 'ledger_index' can be specified.")
            << json;
    }
}

TEST(LedgerSelector, legacy_ledger_index_yields_sequence)
{
    auto const led = parseLedger(R"JSON({ "ledger": 4 })JSON");
    ASSERT_TRUE(led.isSequence());
    EXPECT_EQ(std::get<uint32_t>(led.value), 4u);
}

TEST(LedgerSelector, legacy_ledger_shortcut)
{
    auto const led = parseLedger(R"JSON({ "ledger": "closed" })JSON");
    ASSERT_TRUE(led.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(led.value), LedgerShortcut::Closed);
}

TEST(LedgerSelector, legacy_ledger_hash_yields_hash)
{
    auto const led = parseLedger(
        R"JSON({ "ledger": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789" })JSON");
    ASSERT_TRUE(led.isHash());
    xrpl::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHash64));
    EXPECT_EQ(std::get<xrpl::uint256>(led.value), expected);
}

TEST(LedgerSelector, legacy_ledger_errors_name_the_legacy_field)
{
    struct Case
    {
        char const* json;
        char const* message;
    };
    for (
        auto const& [json, message] : {
            Case{
                R"JSON({ "ledger": "invalid" })JSON",
                "Invalid field 'ledger', not string or number."},
            Case{R"JSON({ "ledger": true })JSON", "Invalid field 'ledger', not string or number."},
            Case{
                R"JSON({ "ledger": "XBCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789" })JSON",
                "Invalid field 'ledger', not hex string."},
        })
    {
        auto value = boost::json::parse(json);
        auto const result = kLedgerSpec.parse(value);
        ASSERT_FALSE(result.has_value()) << json;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << json;
        EXPECT_EQ(result.error().message, message) << json;
    }
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

TEST(LedgerSelector, composes_alongside_other_fields)
{
    auto value = boost::json::parse(
        R"JSON({ "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh", "ledger_index": "validated" })JSON");
    auto const result = kAcctLedgerSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->ledger.isShortcut());
    EXPECT_EQ(std::get<LedgerShortcut>(result->ledger.value), LedgerShortcut::Validated);
}

TEST(LedgerSelector, composed_spec_leaves_ledger_unspecified_when_absent)
{
    auto value =
        boost::json::parse(R"JSON({ "account": "rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh" })JSON");
    auto const result = kAcctLedgerSpec.parse(value);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->ledger.isUnspecified());
}

TEST(LedgerSelector, spec_is_constant_evaluable)
{
    // Forces consteval construction (incl. the boost::pfr completeness guard).
    static constexpr auto kSpec = spec<LedgerOnlyInput>(ledgerSelector(&LedgerOnlyInput::ledger));
    (void)kSpec;
    SUCCEED();
}

TEST(LedgerSpec, ledger_is_validated_before_other_fields)
{
    for (auto const version : {1u, 2u})
    {
        auto value = boost::json::parse(R"JSON({ "ledger_index": "potato", "full": "blah" })JSON");
        auto const result = handlers::ledger::kSpec.parse(value, version);
        ASSERT_FALSE(result.has_value()) << version;
        EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams) << version;
        EXPECT_EQ(result.error().message, "Invalid field 'ledger_index', not string or number.")
            << version;
    }
}

TEST(LedgerSpec, diff_is_not_validated)
{
    for (auto const version : {1u, 2u})
    {
        auto value = boost::json::parse(R"JSON({ "diff": "yes" })JSON");
        EXPECT_TRUE(handlers::ledger::kSpec.parse(value, version).has_value()) << version;
    }
}
