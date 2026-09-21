#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

using namespace rpc::spec;

namespace {

static_assert(
    std::variant_size_v<rpc::CombinedError> == 1,
    "xrpld build: CombinedError must be variant<RippledError> only");
static_assert(std::is_same_v<std::variant_alternative_t<0, rpc::CombinedError>, rpc::RippledError>);

TEST(RpcSpecDSL_Type, StringDirect)
{
    static constexpr auto kSpec = RpcSpec{
        field("name", type<std::string>),
    };

    auto good = boost::json::parse(R"JSON({ "name": "alice" })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());

    auto bad = boost::json::parse(R"JSON({ "name": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSL_Type, DoubleAcceptsDoubleAndRejectsOthers)
{
    static constexpr auto kSpec = RpcSpec{
        field("ratio", type<double>),
    };

    auto good = boost::json::parse(R"JSON({ "ratio": 1.5 })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());

    auto bad = boost::json::parse(R"JSON({ "ratio": "high" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());

    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_Type, Uint32AcceptsInRangeRejectsOthers)
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
    EXPECT_EQ(r1.error(), rpc::RippledError::RpcInvalidParams);

    auto negative = boost::json::parse(R"JSON({ "n": -1 })JSON");
    auto const r2 = kSpec.process(negative);
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r2.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_TypeObject, AcceptsObjectRejectsOthers)
{
    static constexpr auto kSpec = RpcSpec{
        field("entry", type<JsonObject>),
    };

    auto obj = boost::json::parse(R"JSON({ "entry": {} })JSON");
    EXPECT_TRUE(kSpec.process(obj).has_value());

    auto str = boost::json::parse(R"JSON({ "entry": "hello" })JSON");
    auto const result = kSpec.process(str);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());

    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_TypeArray, AcceptsArrayRejectsOthers)
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

TEST(RpcSpecDSL_MultiType, AcceptsFirstType)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto goodInt = boost::json::parse(R"JSON({ "v": 42 })JSON");
    EXPECT_TRUE(kSpec.process(goodInt).has_value());
}

TEST(RpcSpecDSL_MultiType, AcceptsSecondType)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto goodStr = boost::json::parse(R"JSON({ "v": "hello" })JSON");
    EXPECT_TRUE(kSpec.process(goodStr).has_value());
}

TEST(RpcSpecDSL_MultiType, RejectsNeitherType)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto bad = boost::json::parse(R"JSON({ "v": true })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_MultiType, AcceptsObjectWhenIncluded)
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

TEST(RpcSpecDSL_MultiType, AbsentFieldSkipped)
{
    static constexpr auto kSpec = RpcSpec{
        field("v", type<int64_t, std::string>),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_Min, Double)
{
    static constexpr auto kSpec = RpcSpec{
        field("ratio", type<double>, min(0.5)),
    };

    auto bad = boost::json::parse(R"JSON({ "ratio": 0.1 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());

    auto good = boost::json::parse(R"JSON({ "ratio": 1.0 })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());
}

TEST(RpcSpecDSL_Min, Uint32)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<uint32_t>, min(uint32_t{10})),
    };

    auto bad = boost::json::parse(R"JSON({ "n": 5 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);

    auto good = boost::json::parse(R"JSON({ "n": 100 })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());
}

TEST(RpcSpecDSL_Between, Uint32InRangePasses)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto request = boost::json::parse(R"JSON({ "trim": 10 })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_Between, Uint32AtBoundariesPasses)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto lo = boost::json::parse(R"JSON({ "trim": 1 })JSON");
    EXPECT_TRUE(kSpec.process(lo).has_value());

    auto hi = boost::json::parse(R"JSON({ "trim": 25 })JSON");
    EXPECT_TRUE(kSpec.process(hi).has_value());
}

TEST(RpcSpecDSL_Between, Uint32BelowLoFails)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto bad = boost::json::parse(R"JSON({ "trim": 0 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_Between, Uint32AboveHiFails)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", type<uint32_t>, between(uint32_t{1}, uint32_t{25})),
    };
    auto bad = boost::json::parse(R"JSON({ "trim": 26 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_Between, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{
        field("trim", between(uint32_t{1}, uint32_t{25})),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_Int64Boundary, Uint64AboveInt64MaxFailsTypeInt64)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<int64_t>),
    };

    // 2^63 — one above INT64_MAX, parsed as uint64 by boost::json.
    auto request = boost::json::parse(R"JSON({ "n": 9223372036854775808 })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSL_Int64Boundary, Uint64WithinInt64RangePassesTypeInt64)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<int64_t>, min(int64_t{0})),
    };

    // INT64_MAX exactly — boost::json may parse as uint64; must still be accepted.
    auto request = boost::json::parse(R"JSON({ "n": 9223372036854775807 })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_AccountFormat, RejectsInvalidString)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", account),
    };

    auto bad = boost::json::parse(R"JSON({ "account": "rNotAValidAccount" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
    EXPECT_EQ(result.error().message, "accountMalformed");
}

TEST(RpcSpecDSL_AccountFormat, RejectsNonString)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", account),
    };

    auto bad = boost::json::parse(R"JSON({ "account": 12345 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "accountNotString");
}

TEST(RpcSpecDSL_AccountFormat, AbsentFieldAccepted)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", account),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_TimeFormat, ValidIsoStringAccepted)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", type<std::string>, timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({ "date": "2025-05-07T12:34:56Z" })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_TimeFormat, MalformedStringRejected)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({ "date": "not-a-date" })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_TimeFormat, NonStringRejected)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({ "date": 12345 })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_TimeFormat, AbsentFieldAccepted)
{
    static constexpr auto kSpec = RpcSpec{
        field("date", timeFormat("%Y-%m-%dT%TZ")),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_HexString, Uint256AcceptsValidHex)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto good = boost::json::parse(
        R"JSON({ "hash": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());
}

TEST(RpcSpecDSL_HexString, Uint256RejectsMalformedHex)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto bad = boost::json::parse(R"JSON({ "hash": "NOTAHEX" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'hash'.");
}

TEST(RpcSpecDSL_HexString, Uint256RejectsNonString)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto bad = boost::json::parse(R"JSON({ "hash": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'hash'.");
}

TEST(RpcSpecDSL_HexString, AbsentFieldSkipped)
{
    static constexpr auto kSpec = RpcSpec{
        field("hash", uint256Hex),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_Hex256Array, ValidArrayPasses)
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

TEST(RpcSpecDSL_Hex256Array, EmptyArrayPasses)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto empty = boost::json::parse(R"JSON({ "credentials": [] })JSON");
    EXPECT_TRUE(kSpec.process(empty).has_value());
}

TEST(RpcSpecDSL_Hex256Array, InvalidElementFails)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto bad = boost::json::parse(R"JSON({ "credentials": ["NOTAHEX"] })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_Hex256Array, NotAnArrayFails)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto bad = boost::json::parse(R"JSON({ "credentials": "abc" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_Hex256Array, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{field("credentials", hex256Array)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_LedgerIndex, AcceptsPositiveInt)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": 42 })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_LedgerIndex, AcceptsZero)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": 0 })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_LedgerIndex, RejectsNumericStringWithTrailingCharacters)
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

TEST(RpcSpecDSL_LedgerIndex, AcceptsValidatedString)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "validated" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_LedgerIndex, AcceptsNumericString)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "12345" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_LedgerIndex, AcceptsClosedString)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "closed" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_LedgerIndex, AcceptsCurrentString)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "current" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_LedgerIndex, RejectsArbitraryString)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": "invalid" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Invalid field 'ledger_index', not string or number.");
}

TEST(RpcSpecDSL_LedgerIndex, RejectsBool)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto req = boost::json::parse(R"JSON({ "ledger_index": true })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSL_LedgerIndex, AbsentFieldSkipped)
{
    static constexpr auto kSpec = RpcSpec{field("ledger_index", ledgerIndex)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_AccountBase58, AcceptsValidBase58Account)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto req = boost::json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_AccountBase58, RejectsNonString)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto req = boost::json::parse(R"JSON({ "account": 42 })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "accountNotString");
}

TEST(RpcSpecDSL_AccountBase58, RejectsInvalidAccount)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto req = boost::json::parse(R"JSON({ "account": "rNotValid" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_AccountBase58, AbsentFieldSkipped)
{
    static constexpr auto kSpec = RpcSpec{field("account", accountBase58)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_Currency, AcceptsXRP)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": "XRP" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_Currency, AcceptsThreeCharCode)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": "USD" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_Currency, RejectsNonString)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": 42 })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "currencyNotString");
}

TEST(RpcSpecDSL_Currency, RejectsEmpty)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req = boost::json::parse(R"JSON({ "currency": "" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "currencyIsEmpty");
}

TEST(RpcSpecDSL_Currency, RejectsMalformed)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto req =
        boost::json::parse(R"JSON({ "currency": "NOT_VALID_CURRENCY_STRING_TOO_LONG" })JSON");
    auto const result = kSpec.process(req);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_Currency, AbsentFieldSkipped)
{
    static constexpr auto kSpec = RpcSpec{field("currency", currency)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_NotSupported, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{
        field("full", notSupported),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_NotSupported, PresentFieldFails)
{
    static constexpr auto kSpec = RpcSpec{
        field("full", notSupported),
    };
    auto present = boost::json::parse(R"JSON({ "full": true })JSON");
    auto const result = kSpec.process(present);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcNotSupported);
}

// Rippled side of the server-conditional branch. The Clio side lives in its own
// executable (SpecServerConditionalTests.cpp, compiled with RPCSPEC_IS_CLIO);
// together they prove the ifServerClio/ifServerXrpld compile-time switch flips.
TEST(RpcSpecDSL_ServerConditional, IfServerClioValidatorIsInertInRippledBuild)
{
    static constexpr auto kSpec = RpcSpec{
        field("clio_only", ifServerClio(notSupportedIf(true))),
    };
    auto value = boost::json::parse(R"JSON({ "clio_only": true })JSON");
    EXPECT_TRUE(kSpec.process(value).has_value());
}

TEST(RpcSpecDSL_ServerConditional, IfServerXrpldValidatorIsApplied)
{
    static constexpr auto kSpec = RpcSpec{
        field("xrpld_only", ifServerXrpld(notSupportedIf(true))),
    };
    auto bad = boost::json::parse(R"JSON({ "xrpld_only": true })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcNotSupported);
}

TEST(RpcSpecDSL_OneOf, AcceptsValidValue)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto valid = boost::json::parse(R"JSON({ "role": "gateway" })JSON");
    EXPECT_TRUE(kSpec.process(valid).has_value());

    auto valid2 = boost::json::parse(R"JSON({ "role": "user" })JSON");
    EXPECT_TRUE(kSpec.process(valid2).has_value());
}

TEST(RpcSpecDSL_OneOf, RejectsUnknownValue)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto bad = boost::json::parse(R"JSON({ "role": "admin" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_OneOf, RejectsNonString)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto bad = boost::json::parse(R"JSON({ "role": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_OneOf, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{
        field("role", oneOf("gateway", "user")),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_AccountMarker, ValidMarkerPasses)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto request = boost::json::parse(
        R"JSON({ "marker": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA,0" })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_AccountMarker, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_AccountMarker, NotStringFails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(R"JSON({ "marker": 42 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "markerNotString");
}

TEST(RpcSpecDSL_AccountMarker, NoCommaFails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(R"JSON({ "marker": "AABB" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(RpcSpecDSL_AccountMarker, BadHexPartFails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(R"JSON({ "marker": "NOTVALIDHEX,0" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(RpcSpecDSL_AccountMarker, BadHintPartFails)
{
    static constexpr auto kSpec = RpcSpec{field("marker", accountMarker)};
    auto bad = boost::json::parse(
        R"JSON({ "marker": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA,notanumber" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "Invalid field 'marker'.");
}

TEST(RpcSpecDSL_AccountType, ValidTypeStringPasses)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto req = boost::json::parse(R"JSON({ "type": "offer" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_AccountType, UnknownTypeStringFails)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto bad = boost::json::parse(R"JSON({ "type": "not_a_type" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_AccountType, NonStringFails)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto bad = boost::json::parse(R"JSON({ "type": 42 })JSON");
    EXPECT_FALSE(kSpec.process(bad).has_value());
}

TEST(RpcSpecDSL_AccountType, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{field("type", accountType)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_LedgerEntryType, ValidTypeStringPasses)
{
    static constexpr auto kSpec = RpcSpec{field("type", ledgerType)};
    auto req = boost::json::parse(R"JSON({ "type": "state" })JSON");
    EXPECT_TRUE(kSpec.process(req).has_value());
}

TEST(RpcSpecDSL_LedgerEntryType, UnknownTypeStringFails)
{
    static constexpr auto kSpec = RpcSpec{field("type", ledgerType)};
    auto bad = boost::json::parse(R"JSON({ "type": "not_a_type" })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_LedgerEntryType, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{field("type", ledgerType)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_Integration, RippleStatePattern)
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
    EXPECT_EQ(result.error().message, "Required field 'currency' missing");
}

TEST(RpcSpecDSL_Integration, StringOrObjectPattern)
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
