#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Converters.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/Typed.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/Validators.hpp>

#include <Backend.hpp>  // IWYU pragma: keep

#include <cstdint>
#include <expected>
#include <limits>
#include <string>

using namespace rpc::spec;

TEST(RpcSpecDSL_Clamp, Int64MutatesJsonValueInPlace)
{
    static constexpr auto kSpec = RpcSpec{
        field("limit", type<int64_t>, clamp(int64_t{10}, int64_t{400})),
    };

    auto tooLow = boost::json::parse(R"JSON({ "limit": 2 })JSON");
    ASSERT_TRUE(kSpec.process(tooLow).has_value());
    EXPECT_EQ(tooLow.as_object().at("limit").as_int64(), 10);

    auto tooHigh = boost::json::parse(R"JSON({ "limit": 9999 })JSON");
    ASSERT_TRUE(kSpec.process(tooHigh).has_value());
    EXPECT_EQ(tooHigh.as_object().at("limit").as_int64(), 400);

    auto inRange = boost::json::parse(R"JSON({ "limit": 50 })JSON");
    ASSERT_TRUE(kSpec.process(inRange).has_value());
    EXPECT_EQ(inRange.as_object().at("limit").as_int64(), 50);
}

TEST(RpcSpecDSL_Clamp, DoubleClamp)
{
    static constexpr auto kSpec = RpcSpec{
        field("ratio", type<double>, clamp(0.0, 1.0)),
    };

    auto tooLow = boost::json::parse(R"JSON({ "ratio": -0.5 })JSON");
    ASSERT_TRUE(kSpec.process(tooLow).has_value());
    EXPECT_DOUBLE_EQ(tooLow.as_object().at("ratio").as_double(), 0.0);

    auto tooHigh = boost::json::parse(R"JSON({ "ratio": 1.5 })JSON");
    ASSERT_TRUE(kSpec.process(tooHigh).has_value());
    EXPECT_DOUBLE_EQ(tooHigh.as_object().at("ratio").as_double(), 1.0);
}

TEST(RpcSpecDSL_Clamp, Uint32Clamp)
{
    static constexpr auto kSpec = RpcSpec{
        field("n", type<uint32_t>, clamp(uint32_t{10}, uint32_t{400})),
    };

    auto tooLow = boost::json::parse(R"JSON({ "n": 5 })JSON");
    ASSERT_TRUE(kSpec.process(tooLow).has_value());
    EXPECT_EQ(tooLow.as_object().at("n").as_uint64(), 10u);

    auto tooHigh = boost::json::parse(R"JSON({ "n": 9999 })JSON");
    ASSERT_TRUE(kSpec.process(tooHigh).has_value());
    EXPECT_EQ(tooHigh.as_object().at("n").as_uint64(), 400u);
}

TEST(RpcSpecDSL_IfType, SkipsSubValidatorsOnTypeMismatch)
{
    static constexpr auto kSpec = RpcSpec{
        field("value", ifType<int64_t>(min(int64_t{1}))),
    };

    auto request = boost::json::parse(R"JSON({ "value": "hello" })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_IfType, RunsSubValidatorsOnTypeMatch)
{
    static constexpr auto kSpec = RpcSpec{
        field("value", ifType<int64_t>(min(int64_t{1}))),
    };

    auto bad = boost::json::parse(R"JSON({ "value": 0 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());

    auto good = boost::json::parse(R"JSON({ "value": 5 })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());
}

TEST(RpcSpecDSL_IfType, AbsentFieldIsSkipped)
{
    static constexpr auto kSpec = RpcSpec{
        field("value", ifType<int64_t>(min(int64_t{1}))),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_IfType, ModifierMutatesOnTypeMatch)
{
    static constexpr auto kSpec = RpcSpec{
        field("limit", ifType<int64_t>(clamp(int64_t{10}, int64_t{400}))),
    };

    auto request = boost::json::parse(R"JSON({ "limit": 3 })JSON");
    ASSERT_TRUE(kSpec.process(request).has_value());
    EXPECT_EQ(request.as_object().at("limit").as_int64(), 10);
}

TEST(RpcSpecDSL_IfType, ModifierSkipsOnTypeMismatch)
{
    static constexpr auto kSpec = RpcSpec{
        field("limit", ifType<int64_t>(clamp(int64_t{10}, int64_t{400}))),
    };

    auto request = boost::json::parse(R"JSON({ "limit": "default" })JSON");
    ASSERT_TRUE(kSpec.process(request).has_value());
    EXPECT_EQ(request.as_object().at("limit").as_string(), "default");
}

TEST(RpcSpecDSL_IfType, MultipleSubValidatorsAllRun)
{
    static constexpr auto kSpec = RpcSpec{
        field("limit", ifType<int64_t>(min(int64_t{1}), clamp(int64_t{10}, int64_t{400}))),
    };

    auto tooLow = boost::json::parse(R"JSON({ "limit": 0 })JSON");
    EXPECT_FALSE(kSpec.process(tooLow).has_value());

    auto clamped = boost::json::parse(R"JSON({ "limit": 5 })JSON");
    ASSERT_TRUE(kSpec.process(clamped).has_value());
    EXPECT_EQ(clamped.as_object().at("limit").as_int64(), 10);

    auto cappedHigh = boost::json::parse(R"JSON({ "limit": 9999 })JSON");
    ASSERT_TRUE(kSpec.process(cappedHigh).has_value());
    EXPECT_EQ(cappedHigh.as_object().at("limit").as_int64(), 400);
}

TEST(RpcSpecDSL_IfType, StopsAtFirstSubValidatorError)
{
    static constexpr auto kSpec = RpcSpec{
        field("value", ifType<int64_t>(min(int64_t{5}), min(int64_t{10}))),
    };

    auto request = boost::json::parse(R"JSON({ "value": 3 })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSL_IfType, UnionTypeLedgerIndex)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", required, account),
        field("ledger_index", ifType<int64_t>(min(int64_t{0})), ifType<std::string>()),
    };

    auto intValid = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": 42 })JSON");
    EXPECT_TRUE(kSpec.process(intValid).has_value());

    auto intNeg = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": -1 })JSON");
    EXPECT_FALSE(kSpec.process(intNeg).has_value());

    auto strValid = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": "validated" })JSON");
    EXPECT_TRUE(kSpec.process(strValid).has_value());

    auto absent =
        boost::json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());

    auto wrongType = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": true })JSON");
    EXPECT_TRUE(kSpec.process(wrongType).has_value());
}

TEST(RpcSpecDSL_IfType, PipeStyle)
{
    static constexpr auto kSpec = RpcSpec{
        field("account") | required | account,
        field("ledger_index") | ifType<int64_t>(min(int64_t{0})),
    };

    auto valid = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": 100 })JSON");
    EXPECT_TRUE(kSpec.process(valid).has_value());

    auto invalid = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": -5 })JSON");
    EXPECT_FALSE(kSpec.process(invalid).has_value());
}

TEST(RpcSpecDSL_IfType, CombinedWithOtherValidators)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", required, account),
        field(
            "limit", required, ifType<int64_t>(min(int64_t{1}), clamp(int64_t{10}, int64_t{400}))),
    };

    auto noLimit =
        boost::json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })JSON");
    auto const result = kSpec.process(noLimit);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Required field 'limit' missing");

    auto strLimit = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": "max" })JSON");
    EXPECT_TRUE(kSpec.process(strLimit).has_value());

    auto good = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": 50 })JSON");
    ASSERT_TRUE(kSpec.process(good).has_value());
    EXPECT_EQ(good.as_object().at("limit").as_int64(), 50);

    auto low = boost::json::parse(
        R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": 2 })JSON");
    ASSERT_TRUE(kSpec.process(low).has_value());
    EXPECT_EQ(low.as_object().at("limit").as_int64(), 10);
}

TEST(RpcSpecDSL_IfType, PipeStyleWithSubItems)
{
    static constexpr auto kSpec = RpcSpec{
        field("limit") | ifType<int64_t>(min(int64_t{1}), clamp(int64_t{10}, int64_t{400})),
    };

    auto low = boost::json::parse(R"JSON({ "limit": 5 })JSON");
    ASSERT_TRUE(kSpec.process(low).has_value());
    EXPECT_EQ(low.as_object().at("limit").as_int64(), 10);

    auto bad = boost::json::parse(R"JSON({ "limit": 0 })JSON");
    EXPECT_FALSE(kSpec.process(bad).has_value());
}

TEST(RpcSpecDSL_Section, ValidSubObjectPasses)
{
    static constexpr auto kSpec = RpcSpec{
        field(
            "taker_pays",
            section(
                field("currency", required, type<std::string>), field("value", type<std::string>))),
    };

    auto request =
        boost::json::parse(R"JSON({ "taker_pays": { "currency": "XRP", "value": "1" } })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_Section, MissingRequiredSubFieldFails)
{
    static constexpr auto kSpec = RpcSpec{
        field("taker_pays", section(field("currency", required))),
    };

    auto request = boost::json::parse(R"JSON({ "taker_pays": {} })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Required field 'currency' missing");
}

TEST(RpcSpecDSL_Section, WrongSubFieldTypeFails)
{
    static constexpr auto kSpec = RpcSpec{
        field("taker_pays", section(field("currency", required, type<std::string>))),
    };

    auto request = boost::json::parse(R"JSON({ "taker_pays": { "currency": 42 } })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_Section, AbsentParentFieldSkipsSection)
{
    static constexpr auto kSpec = RpcSpec{
        field("taker_pays", section(field("currency", required))),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_Section, NonObjectParentFieldFails)
{
    static constexpr auto kSpec = RpcSpec{
        field("taker_pays", section(field("currency", required))),
    };

    auto request = boost::json::parse(R"JSON({ "taker_pays": "XRP" })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_Section, ModifierMutatesSubField)
{
    static constexpr auto kSpec = RpcSpec{
        field("options", section(field("limit", type<int64_t>, clamp(int64_t{10}, int64_t{400})))),
    };

    auto request = boost::json::parse(R"JSON({ "options": { "limit": 3 } })JSON");
    ASSERT_TRUE(kSpec.process(request).has_value());
    EXPECT_EQ(request.as_object().at("options").as_object().at("limit").as_int64(), 10);
}

TEST(RpcSpecDSL_Section, PipeStyle)
{
    static constexpr auto kSpec = RpcSpec{
        field("payload") |
            section(
                field("type", required, type<std::string>),
                field("value", required, type<int64_t>)),
    };

    auto good = boost::json::parse(R"JSON({ "payload": { "type": "foo", "value": 1 } })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());

    auto bad = boost::json::parse(R"JSON({ "payload": { "type": "foo" } })JSON");
    EXPECT_FALSE(kSpec.process(bad).has_value());
}

TEST(RpcSpecDSL_IfObject, SkipsWhenFieldIsNotObject)
{
    static constexpr auto kSpec = RpcSpec{
        field("entry", ifType<JsonObject>(section(field("a", required)))),
    };

    auto request = boost::json::parse(R"JSON({ "entry": "validated" })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_IfObject, RunsSectionWhenFieldIsObject)
{
    static constexpr auto kSpec = RpcSpec{
        field("entry", ifType<JsonObject>(section(field("a", required, type<std::string>)))),
    };

    auto good = boost::json::parse(R"JSON({ "entry": { "a": "hello" } })JSON");
    EXPECT_TRUE(kSpec.process(good).has_value());

    auto bad = boost::json::parse(R"JSON({ "entry": {} })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "Required field 'a' missing");
}

TEST(RpcSpecDSL_IfObject, AbsentFieldSkipped)
{
    static constexpr auto kSpec = RpcSpec{
        field("entry", ifType<JsonObject>(section(field("a", required)))),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_IfArray, SkipsWhenFieldIsNotArray)
{
    static constexpr auto kSpec = RpcSpec{
        field("ids", ifType<JsonArray>(ifType<int64_t>())),
    };

    auto request = boost::json::parse(R"JSON({ "ids": {} })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_IfArray, RunsSubProcessorsWhenFieldIsArray)
{
    static constexpr auto kSpec = RpcSpec{
        field("ids", ifType<JsonArray>(ifType<int64_t>())),
    };

    auto request = boost::json::parse(R"JSON({ "ids": [1, 2, 3] })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_IfArray, AbsentFieldSkipped)
{
    static constexpr auto kSpec = RpcSpec{
        field("ids", ifType<JsonArray>(ifType<int64_t>())),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_WithCustomError, OverridesCodeOnRequirementFailure)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", withCustomError(required, rpc::RippledError::RpcActMalformed)),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcActMalformed);
    EXPECT_TRUE(result.error().message.empty());
}

TEST(RpcSpecDSL_WithCustomError, PassesThroughWhenWrappedSucceeds)
{
    static constexpr auto kSpec = RpcSpec{
        field("account", withCustomError(required, rpc::RippledError::RpcActMalformed)),
    };

    auto request =
        boost::json::parse(R"JSON({ "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn" })JSON");
    EXPECT_TRUE(kSpec.process(request).has_value());
}

TEST(RpcSpecDSL_WithCustomError, AppendsCustomMessageOnFailure)
{
    static constexpr auto kSpec = RpcSpec{
        field(
            "marker",
            withCustomError(required, rpc::RippledError::RpcInvalidParams, "invalidMarker")),
    };

    auto request = boost::json::parse(R"JSON({})JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "invalidMarker");
}

TEST(RpcSpecDSL_WithCustomError, ModifierPathOverridesCode)
{
    // Wrapping IfType (a SomeModifier) — sub-validator failure must surface as the custom code.
    static constexpr auto kSpec = RpcSpec{
        field(
            "limit",
            withCustomError(
                ifType<int64_t>(min(int64_t{1})), rpc::RippledError::RpcInvalidParams, "tooLow")),
    };

    auto bad = boost::json::parse(R"JSON({ "limit": 0 })JSON");
    auto const result = kSpec.process(bad);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
    EXPECT_EQ(result.error().message, "tooLow");

    auto skipped = boost::json::parse(R"JSON({ "limit": "default" })JSON");
    EXPECT_TRUE(kSpec.process(skipped).has_value());
}

TEST(RpcSpecDSL_CustomModifier, LambdaInvokedWhenPresent)
{
    static constexpr auto kSpec = RpcSpec{
        field("val", customModifier([](auto& fieldView) -> rpc::spec::MaybeError {
                  fieldView.set(int64_t{99});
                  return {};
              })),
    };
    auto request = boost::json::parse(R"JSON({ "val": 1 })JSON");
    ASSERT_TRUE(kSpec.process(request).has_value());
    EXPECT_EQ(request.as_object().at("val").as_int64(), 99);
}

TEST(RpcSpecDSL_CustomModifier, LambdaNotInvokedWhenAbsent)
{
    static constexpr auto kSpec = RpcSpec{
        field("val", customModifier([](auto& fieldView) -> rpc::spec::MaybeError {
                  fieldView.set(int64_t{99});
                  return {};
              })),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_CustomModifier, LambdaCanReturnError)
{
    static constexpr auto kSpec = RpcSpec{
        field("val", customModifier([](auto& /*f*/) -> rpc::spec::MaybeError {
                  return std::unexpected{rpc::Status{rpc::RippledError::RpcInvalidParams}};
              })),
    };
    auto request = boost::json::parse(R"JSON({ "val": 1 })JSON");
    auto const result = kSpec.process(request);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}

TEST(RpcSpecDSL_ToLower, ConvertsToLowercase)
{
    static constexpr auto kSpec = RpcSpec{
        field("tx_type", toLower),
    };
    auto request = boost::json::parse(R"JSON({ "tx_type": "Payment" })JSON");
    ASSERT_TRUE(kSpec.process(request).has_value());
    EXPECT_EQ(request.as_object().at("tx_type").as_string(), "payment");
}

TEST(RpcSpecDSL_ToLower, AlreadyLowercaseUnchanged)
{
    static constexpr auto kSpec = RpcSpec{
        field("tx_type", toLower),
    };
    auto request = boost::json::parse(R"JSON({ "tx_type": "payment" })JSON");
    ASSERT_TRUE(kSpec.process(request).has_value());
    EXPECT_EQ(request.as_object().at("tx_type").as_string(), "payment");
}

TEST(RpcSpecDSL_ToLower, AbsentFieldNoOp)
{
    static constexpr auto kSpec = RpcSpec{
        field("tx_type", toLower),
    };
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

TEST(RpcSpecDSL_ToLower, NonStringNoOp)
{
    static constexpr auto kSpec = RpcSpec{
        field("tx_type", toLower),
    };
    auto num = boost::json::parse(R"JSON({ "tx_type": 42 })JSON");
    ASSERT_TRUE(kSpec.process(num).has_value());
    EXPECT_TRUE(num.as_object().at("tx_type").is_int64());
}

TEST(RpcSpecDSL_ClampAs, Int32OverflowClampedToMax)
{
    static constexpr auto kSpec = RpcSpec{field("v", type<int64_t>, clampAs<int32_t>)};
    auto j = boost::json::parse(R"JSON({ "v": 4294967296 })JSON");
    EXPECT_TRUE(kSpec.process(j).has_value());
    EXPECT_EQ(j.as_object().at("v").as_int64(), std::numeric_limits<int32_t>::max());
}

TEST(RpcSpecDSL_ClampAs, Int32UnderflowClampedToMin)
{
    static constexpr auto kSpec = RpcSpec{field("v", type<int64_t>, clampAs<int32_t>)};
    auto j = boost::json::parse(R"JSON({ "v": -4294967296 })JSON");
    EXPECT_TRUE(kSpec.process(j).has_value());
    EXPECT_EQ(j.as_object().at("v").as_int64(), std::numeric_limits<int32_t>::min());
}

TEST(RpcSpecDSL_ClampAs, Int32InRangeUnchanged)
{
    static constexpr auto kSpec = RpcSpec{field("v", type<int64_t>, clampAs<int32_t>)};
    auto j = boost::json::parse(R"JSON({ "v": 12345 })JSON");
    EXPECT_TRUE(kSpec.process(j).has_value());
    EXPECT_EQ(j.as_object().at("v").as_int64(), 12345);
}

TEST(RpcSpecDSL_ClampAs, Uint32OverflowClampedToMax)
{
    static constexpr auto kSpec = RpcSpec{field("v", type<int64_t>, clampAs<uint32_t>)};
    auto j = boost::json::parse(R"JSON({ "v": 8589934592 })JSON");
    EXPECT_TRUE(kSpec.process(j).has_value());
    EXPECT_EQ(
        j.as_object().at("v").as_uint64(),
        static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()));
}

TEST(RpcSpecDSL_ClampAs, Uint32NegativeClampedToZero)
{
    static constexpr auto kSpec = RpcSpec{field("v", type<int64_t>, clampAs<uint32_t>)};
    auto j = boost::json::parse(R"JSON({ "v": -5 })JSON");
    EXPECT_TRUE(kSpec.process(j).has_value());
    EXPECT_EQ(j.as_object().at("v").as_uint64(), 0u);
}

TEST(RpcSpecDSL_ClampAs, AbsentFieldPasses)
{
    static constexpr auto kSpec = RpcSpec{field("v", clampAs<int32_t>)};
    auto absent = boost::json::parse(R"JSON({})JSON");
    EXPECT_TRUE(kSpec.process(absent).has_value());
}

namespace {
struct TypedLimitInput
{
    uint32_t limit = 0;
};
struct TypedTxInput
{
    std::string txType;
};
}  // namespace

TEST(TypedSpecModifier, ClampRunsBeforeConverter)
{
    static constexpr auto kSpec = spec<TypedLimitInput>(
        field("limit", &TypedLimitInput::limit, clamp(uint32_t{10}, uint32_t{400}), asUint32));

    auto tooLow = boost::json::parse(R"JSON({ "limit": 5 })JSON");
    auto const low = kSpec.parse(tooLow);
    ASSERT_TRUE(low.has_value());
    EXPECT_EQ(low->limit, 10u);  // clamped up, then converted

    auto tooHigh = boost::json::parse(R"JSON({ "limit": 9999 })JSON");
    auto const high = kSpec.parse(tooHigh);
    ASSERT_TRUE(high.has_value());
    EXPECT_EQ(high->limit, 400u);  // clamped down, then converted

    auto inRange = boost::json::parse(R"JSON({ "limit": 50 })JSON");
    auto const ok = kSpec.parse(inRange);
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok->limit, 50u);
}

TEST(TypedSpecModifier, ToLowerRunsBeforeConverter)
{
    static constexpr auto kSpec =
        spec<TypedTxInput>(field("tx_type", &TypedTxInput::txType, toLower, asString));

    auto request = boost::json::parse(R"JSON({ "tx_type": "Payment" })JSON");
    auto const result = kSpec.parse(request);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->txType, "payment");  // lowercased by the modifier, then converted
}

TEST(TypedSpecModifier, ConverterValidatesModifiedValue)
{
    // The converter still rejects values the modifier left invalid.
    static constexpr auto kSpec = spec<TypedLimitInput>(
        field("limit", &TypedLimitInput::limit, clamp(uint32_t{10}, uint32_t{400}), asUint32));

    auto wrongType = boost::json::parse(R"JSON({ "limit": "not a number" })JSON");
    auto const result = kSpec.parse(wrongType);  // clamp no-ops on non-uint, converter rejects
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), rpc::RippledError::RpcInvalidParams);
}
