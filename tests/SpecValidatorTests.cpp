#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

using namespace rpc::spec;

namespace {

static_assert(
    std::variant_size_v<rpc::CombinedError> == 1,
    "xrpld build: CombinedError must be variant<XrpldError> only");
static_assert(std::is_same_v<std::variant_alternative_t<0, rpc::CombinedError>, rpc::XrpldError>);

TEST(RpcSpecDSLType, string_direct)
{
    static constexpr auto kSpec = RpcSpec{
        field("name", type<std::string>),
    };

    auto good = boost::json::parse(R"JSON({ "name": "alice" })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());

    auto bad = boost::json::parse(R"JSON({ "name": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSLType, double_accepts_double_and_rejects_others)
{
    static constexpr auto kSpec = RpcSpec{
        field("ratio", type<double>),
    };

    auto good = boost::json::parse(R"JSON({ "ratio": 1.5 })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());

    auto bad = boost::json::parse(R"JSON({ "ratio": "high" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());

    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLType, uint32_accepts_in_range_rejects_others)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<uint32_t>),
    };

    auto positive = boost::json::parse(R"JSON({ "n": 42 })JSON");
    EXPECT_TRUE(kSpec.process(positive).has_value());

    auto maxU32 = boost::json::parse(R"JSON({ "n": 4294967295 })JSON");
    EXPECT_TRUE(kSpec.process(maxU32).has_value());

    auto overflow = boost::json::parse(R"JSON({ "n": 4294967296 })JSON");
    auto const r1 = kSpec.process(overflow);
    ASSERT_FALSE(r1.has_value());
    EXPECT_EQ(r1.error(), rpc::XrpldError::RpcInvalidParams);

    auto negative = boost::json::parse(R"JSON({ "n": -1 })JSON");
    auto const r2 = kSpec.process(negative);
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r2.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLTypeObject, accepts_object_rejects_others)
{
    static constexpr auto kSpec = RpcSpec{
        field("entry", type<JsonObject>),
    };

    auto obj = boost::json::parse(R"JSON({ "entry": {} })JSON");
    EXPECT_TRUE(kSpec.process(obj).has_value());

    auto str = boost::json::parse(R"JSON({ "entry": "hello" })JSON");
    auto const result = kSpec.process(str);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());

    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLTypeArray, accepts_array_rejects_others)
{
    static constexpr auto kSpec = RpcSpec{
        field("ids", type<JsonArray>),
    };

    auto arr = boost::json::parse(R"JSON({ "ids": [1, 2] })JSON");
    EXPECT_TRUE(kSpec.process(arr).has_value());

    auto str = boost::json::parse(R"JSON({ "ids": "hello" })JSON");
    EXPECT_FALSE(kSpec.process(str).has_value());

    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLMultiType, accepts_first_type)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto goodInt = boost::json::parse(R"JSON({ "v": 42 })JSON");
    EXPECT_TRUE(kSpec.process(goodInt).has_value());
}

TEST(RpcSpecDSLMultiType, accepts_second_type)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto goodStr = boost::json::parse(R"JSON({ "v": "hello" })JSON");
    EXPECT_TRUE(kSpec.process(goodStr).has_value());
}

TEST(RpcSpecDSLMultiType, rejects_neither_type)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto bad = boost::json::parse(R"JSON({ "v": true })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLMultiType, accepts_object_when_included)
{
    static constexpr auto kSpec = RpcSpec{
        field("entry", type<std::string, JsonObject>),
    };
    auto str = boost::json::parse(R"JSON({ "entry": "abc" })JSON");
    EXPECT_TRUE(kSpec.process(str).has_value());

    auto obj = boost::json::parse(R"JSON({ "entry": {} })JSON");
    EXPECT_TRUE(kSpec.process(obj).has_value());

    auto num = boost::json::parse(R"JSON({ "entry": 42 })JSON");
    EXPECT_FALSE(kSpec.process(num).has_value());
}

TEST(RpcSpecDSLMultiType, absent_field_skipped)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLMin, double)
{
    static constexpr auto kSpec = RpcSpec{
        field("ratio", type<double>, min(0.5)),
    };

    auto bad = boost::json::parse(R"JSON({ "ratio": 0.1 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());

    auto good = boost::json::parse(R"JSON({ "ratio": 1.0 })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());
}

TEST(RpcSpecDSLMin, uint32)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<uint32_t>, min(uint32_t{10})),
    };

    auto bad = boost::json::parse(R"JSON({ "n": 5 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);

    auto good = boost::json::parse(R"JSON({ "n": 100 })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());
}

TEST(RpcSpecDSLBetween, uint32_in_range_passes)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto request = boost::json::parse(R"JSON({ "trim": 10 })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSLBetween, uint32_at_boundaries_passes)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto lo = boost::json::parse(R"JSON({ "trim": 1 })JSON");
    EXPECT_TRUE(kSpec.process(lo).has_value());

    auto hi = boost::json::parse(R"JSON({ "trim": 25 })JSON");
    EXPECT_TRUE(kSpec.process(hi).has_value());
}

TEST(RpcSpecDSLBetween, uint32_below_lo_fails)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto bad = boost::json::parse(R"JSON({ "trim": 0 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLBetween, uint32_above_hi_fails)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto bad = boost::json::parse(R"JSON({ "trim": 26 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLBetween, absent_field_passes)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", between(uint32_t{1}, uint32_t{25})),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLInt64Boundary, uint64_above_int64_max_fails_type_int64)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<int64_t>),
    };

    // 2^63 — one above INT64_MAX, parsed as uint64 by boost::json.
    auto request = boost::json::parse(R"JSON({ "n": 9223372036854775808 })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSLInt64Boundary, uint64_within_int64_range_passes_type_int64)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<int64_t>, min(int64_t{0})),
    };

    // INT64_MAX exactly — boost::json may parse as uint64; must still be accepted.
    auto request = boost::json::parse(R"JSON({ "n": 9223372036854775807 })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSLAccountFormat, rejects_invalid_string)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", account),
    };

    auto bad = boost::json::parse(R"JSON({ "account": "rNotAValidAccount" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "accountMalformed");
}

TEST(RpcSpecDSLAccountFormat, rejects_non_string)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", account),
    };

    auto bad = boost::json::parse(R"JSON({ "account": 12345 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "accountNotString");
}

TEST(RpcSpecDSLAccountFormat, absent_field_accepted)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", account),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSLTimeFormat, valid_iso_string_accepted)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", type<std::string>, timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({ "date": "2025-05-07T12:34:56Z" })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSLTimeFormat, malformed_string_rejected)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({ "date": "not-a-date" })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLTimeFormat, non_string_rejected)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({ "date": 12345 })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLTimeFormat, absent_field_accepted)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSLHexString, uint256_accepts_valid_hex)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto good = boost::json::parse(
        R"JSON({ "hash": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());
}

TEST(RpcSpecDSLHexString, uint256_rejects_malformed_hex)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto bad = boost::json::parse(R"JSON({ "hash": "NOTAHEX" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'hash'.");
}

TEST(RpcSpecDSLHexString, uint256_rejects_non_string)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto bad = boost::json::parse(R"JSON({ "hash": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'hash'.");
}

TEST(RpcSpecDSLHexString, absent_field_skipped)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLHex256Array, valid_array_passes)
{
    static constexpr auto kSpec = RpcSpec{
        field("credentials", hex256Array),
    };
    auto request = boost::json::parse(
        R"JSON({ "credentials": [
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        ] })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSLHex256Array, empty_array_passes)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto empty = boost::json::parse(R"JSON({ "credentials": [] })JSON");
    EXPECT_TRUE(kSpec.process(empty).has_value());
}

TEST(RpcSpecDSLHex256Array, invalid_element_fails)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto bad = boost::json::parse(R"JSON({ "credentials": ["NOTAHEX"] })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLHex256Array, not_an_array_fails)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto bad = boost::json::parse(R"JSON({ "credentials": "abc" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLHex256Array, absent_field_passes)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLLedgerIndex, accepts_positive_int)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": 42 })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLLedgerIndex, accepts_zero)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": 0 })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLLedgerIndex, rejects_numeric_string_with_trailing_characters)
{
    // checkIsU32Numeric must consume the whole string: a partial parse like "12abc" used to be
    // accepted here while ledgerSpecifierFromIndex (the typed path) rejected it, so the
    // validate-only and parse paths disagreed on the same input.
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    for (auto const* json :
         {R"JSON({ "ledger_index": "12abc" })JSON",
          R"JSON({ "ledger_index": "1 " })JSON",
          R"JSON({ "ledger_index": "0x10" })JSON"})
    {
        auto req = boost::json::parse(json);
        EXPECT_FALSE(kSpec.process(req).has_value()) << json;
    }
}

TEST(RpcSpecDSLLedgerIndex, accepts_validated_string)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "validated" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLLedgerIndex, accepts_numeric_string)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "12345" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLLedgerIndex, accepts_closed_string)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "closed" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLLedgerIndex, accepts_current_string)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "current" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLLedgerIndex, rejects_arbitrary_string)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "invalid" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'ledger_index', not string or number.");
}

TEST(RpcSpecDSLLedgerIndex, rejects_bool)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": true })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSLLedgerIndex, absent_field_skipped)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLAccountBase58, accepts_valid_base58_account)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto req = boost::json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLAccountBase58, rejects_non_string)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto req = boost::json::parse(R"JSON({ "account": 42 })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "accountNotString");
}

TEST(RpcSpecDSLAccountBase58, rejects_invalid_account)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto req = boost::json::parse(R"JSON({ "account": "rNotValid" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLAccountBase58, absent_field_skipped)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLCurrency, accepts_xrp)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": "XRP" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLCurrency, accepts_three_char_code)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": "USD" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLCurrency, rejects_non_string)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": 42 })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "currencyNotString");
}

TEST(RpcSpecDSLCurrency, rejects_empty)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": "" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "currencyIsEmpty");
}

TEST(RpcSpecDSLCurrency, rejects_malformed)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req =
        boost::json::parse(R"JSON({ "currency": "NOT_VALID_CURRENCY_STRING_TOO_LONG" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLCurrency, absent_field_skipped)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLNotSupported, absent_field_passes)
{
    static constexpr auto kSpec = RpcSpec{
        field("full", notSupported),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLNotSupported, present_field_fails)
{
    static constexpr auto kSpec = RpcSpec{
        field("full", notSupported),
    };
    auto present = boost::json::parse(R"JSON({ "full": true })JSON");
    auto const result = kSpec.process(present);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcNotSupported);
}

// Rippled side of the server-conditional branch. The Clio side lives in its own
// executable (SpecServerConditionalTests.cpp, compiled with RPCSPEC_IS_CLIO);
// together they prove the ifServerClio/ifServerXrpld compile-time switch flips.
TEST(RpcSpecDSLServerConditional, if_server_clio_validator_is_inert_in_rippled_build)
{
    static constexpr auto kSpec = RpcSpec{
        field("clio_only", ifServerClio(notSupportedIf(true))),
    };
    auto value = boost::json::parse(R"JSON({ "clio_only": true })JSON");
    EXPECT_TRUE(kSpec.process(value).has_value());
}

TEST(RpcSpecDSLServerConditional, if_server_xrpld_validator_is_applied)
{
    static constexpr auto kSpec = RpcSpec{
        field("xrpld_only", ifServerXrpld(notSupportedIf(true))),
    };
    auto bad = boost::json::parse(R"JSON({ "xrpld_only": true })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcNotSupported);
}

TEST(RpcSpecDSLOneOf, accepts_valid_value)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto valid = boost::json::parse(R"JSON({ "role": "gateway" })JSON");
    EXPECT_TRUE(kSpec.process(valid).has_value());

    auto valid2 = boost::json::parse(R"JSON({ "role": "user" })JSON");
    EXPECT_TRUE(kSpec.process(valid2).has_value());
}

TEST(RpcSpecDSLOneOf, rejects_unknown_value)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto bad = boost::json::parse(R"JSON({ "role": "admin" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLOneOf, rejects_non_string)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto bad = boost::json::parse(R"JSON({ "role": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLOneOf, absent_field_passes)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLAccountMarker, valid_marker_passes)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto request = boost::json::parse(
        R"JSON({ "marker": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA,0" })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSLAccountMarker, absent_field_passes)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLAccountMarker, not_string_fails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(R"JSON({ "marker": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerNotString");
}

TEST(RpcSpecDSLAccountMarker, no_comma_fails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(R"JSON({ "marker": "AABB" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(RpcSpecDSLAccountMarker, bad_hex_part_fails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(R"JSON({ "marker": "NOTVALIDHEX,0" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(RpcSpecDSLAccountMarker, bad_hint_part_fails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(
        R"JSON({ "marker": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA,notanumber" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(RpcSpecDSLAccountType, valid_type_string_passes)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto req = boost::json::parse(R"JSON({ "type": "offer" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLAccountType, unknown_type_string_fails)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto bad = boost::json::parse(R"JSON({ "type": "not_a_type" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLAccountType, non_string_fails)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto bad = boost::json::parse(R"JSON({ "type": 42 })JSON");
    EXPECT_FALSE(kSpec.process(bad).has_value());
}

TEST(RpcSpecDSLAccountType, absent_field_passes)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLLedgerEntryType, valid_type_string_passes)
{
    static constexpr auto kSpec = RpcSpec{field("type", ledgerType)};
    auto req = boost::json::parse(R"JSON({ "type": "state" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSLLedgerEntryType, unknown_type_string_fails)
{
    static constexpr auto kSpec = RpcSpec{field("type", ledgerType)};
    auto bad = boost::json::parse(R"JSON({ "type": "not_a_type" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::XrpldError::RpcInvalidParams);
}

TEST(RpcSpecDSLLedgerEntryType, absent_field_passes)
{
    static constexpr auto kSpec = RpcSpec{field("type", ledgerType)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSLIntegration, ripple_state_pattern)
{
    static constexpr auto kSpec = RpcSpec{
        field(
            "ripple_state",
            type<JsonObject>,
            section(
                field("currency", required, currency), field("account", required, accountBase58))),
    };

    auto good = boost::json::parse(
        R"JSON({ "ripple_state": { "currency": "USD", "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" } })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());

    auto missingCurrency = boost::json::parse(
        R"JSON({ "ripple_state": { "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" } })JSON");
    auto const result = kSpec.process(missingCurrency);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Missing field 'currency'.");
}

TEST(RpcSpecDSLIntegration, string_or_object_pattern)
{
    // Mimics fields like "offer": string hex OR object {account, seq}
    static constexpr auto kSpec = RpcSpec{
        field(
            "offer",
            type<std::string, JsonObject>,
            ifType<std::string>(uint256Hex),
            ifType<JsonObject>(section(
                field("account", required, accountBase58),
                field("seq", required, type<uint32_t>)))),
    };

    auto hex = boost::json::parse(
        R"JSON({ "offer": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" })JSON");
    EXPECT_TRUE(kSpec.process(hex).has_value());

    auto obj = boost::json::parse(
        R"JSON({ "offer": { "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "seq": 1 } })JSON");
    EXPECT_TRUE(kSpec.process(obj).has_value());

    auto badType = boost::json::parse(R"JSON({ "offer": 42 })JSON");
    EXPECT_FALSE(kSpec.process(badType).has_value());
}

}  // namespace
