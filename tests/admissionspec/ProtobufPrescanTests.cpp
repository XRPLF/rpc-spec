#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/AdmissionSpec.hpp>
#include <admissionspec/BaseLimits.hpp>
#include <admissionspec/BucketSettings.hpp>
#include <admissionspec/ConnectionLimiter.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/PenaltyCosts.hpp>
#include <admissionspec/Pipeline.hpp>
#include <admissionspec/ProtobufPrescan.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/SizeCostRamp.hpp>
#include <admissionspec/SpecAttributes.hpp>
#include <admissionspec/SpecLookup.hpp>
#include <admissionspec/Tunable.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message.h>
#include <google/protobuf/wire_format_lite.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

using namespace admission::spec;

namespace {

// enum Color { RED = 0; GREEN = 1; }
// message Sub { optional int32 x = 1; optional Sub child = 2; }
// message Test {
//   optional uint32 a = 1; repeated bytes items = 2; optional Sub sub = 3;
//   optional Color color = 4; repeated uint32 nums = 5 [packed = true]; optional fixed64 f = 6;
//   optional int32 i = 7; optional sint64 s = 8; optional bool b = 9; optional double d = 10;
//   optional string name = 11; optional float fl = 12;
// }
class Schema
{
    google::protobuf::DescriptorPool pool_;
    google::protobuf::DynamicMessageFactory factory_{&pool_};

public:
    google::protobuf::Descriptor const* test{};

    Schema()
    {
        auto file = google::protobuf::FileDescriptorProto{};
        file.set_name("prescan_test.proto");
        file.set_package("prescan");
        file.set_syntax("proto2");

        auto& color = *file.add_enum_type();
        color.set_name("Color");
        auto& red = *color.add_value();
        red.set_name("RED");
        red.set_number(0);
        auto& green = *color.add_value();
        green.set_name("GREEN");
        green.set_number(1);

        auto const add =
            [](google::protobuf::DescriptorProto& message,
               std::string const& name,
               int32_t number,
               google::protobuf::FieldDescriptorProto::Type type,
               google::protobuf::FieldDescriptorProto::Label label,
               std::string const& typeName = {}) -> google::protobuf::FieldDescriptorProto& {
            auto& field = *message.add_field();
            field.set_name(name);
            field.set_number(number);
            field.set_type(type);
            field.set_label(label);
            if (not typeName.empty())
            {
                field.set_type_name(typeName);
            }
            return field;
        };

        auto& sub = *file.add_message_type();
        sub.set_name("Sub");
        add(sub,
            "x",
            1,
            google::protobuf::FieldDescriptorProto::TYPE_INT32,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(sub,
            "child",
            2,
            google::protobuf::FieldDescriptorProto::TYPE_MESSAGE,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL,
            ".prescan.Sub");

        auto& test = *file.add_message_type();
        test.set_name("Test");
        add(test,
            "a",
            1,
            google::protobuf::FieldDescriptorProto::TYPE_UINT32,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(test,
            "items",
            2,
            google::protobuf::FieldDescriptorProto::TYPE_BYTES,
            google::protobuf::FieldDescriptorProto::LABEL_REPEATED);
        add(test,
            "sub",
            3,
            google::protobuf::FieldDescriptorProto::TYPE_MESSAGE,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL,
            ".prescan.Sub");
        add(test,
            "color",
            4,
            google::protobuf::FieldDescriptorProto::TYPE_ENUM,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL,
            ".prescan.Color");
        add(test,
            "nums",
            5,
            google::protobuf::FieldDescriptorProto::TYPE_UINT32,
            google::protobuf::FieldDescriptorProto::LABEL_REPEATED)
            .mutable_options()
            ->set_packed(true);
        add(test,
            "f",
            6,
            google::protobuf::FieldDescriptorProto::TYPE_FIXED64,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(test,
            "i",
            7,
            google::protobuf::FieldDescriptorProto::TYPE_INT32,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(test,
            "s",
            8,
            google::protobuf::FieldDescriptorProto::TYPE_SINT64,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(test,
            "b",
            9,
            google::protobuf::FieldDescriptorProto::TYPE_BOOL,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(test,
            "d",
            10,
            google::protobuf::FieldDescriptorProto::TYPE_DOUBLE,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(test,
            "name",
            11,
            google::protobuf::FieldDescriptorProto::TYPE_STRING,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
        add(test,
            "fl",
            12,
            google::protobuf::FieldDescriptorProto::TYPE_FLOAT,
            google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);

        if (pool_.BuildFile(file) == nullptr)
        {
            throw std::runtime_error("prescan test schema did not build");
        }
        this->test = pool_.FindMessageTypeByName("prescan.Test");
    }

    // Whether libprotobuf's parser accepts `bytes` as a Test.
    [[nodiscard]] bool
    parses(std::string const& bytes)
    {
        auto const message =
            std::unique_ptr<google::protobuf::Message>{factory_.GetPrototype(test)->New()};
        return message->ParsePartialFromArray(bytes.data(), static_cast<int>(bytes.size()));
    }
};

Schema&
schema()
{
    static auto instance = Schema{};
    return instance;
}

// Builds a payload field by field.
class Writer
{
    std::string bytes_;

public:
    Writer&
    varint(int32_t field, uint64_t value)
    {
        tag(field, google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT);
        raw(varintBytes(value));
        return *this;
    }

    Writer&
    fixed64(int32_t field, uint64_t value)
    {
        tag(field, google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED64);
        for (uint32_t i{}; i < 8; ++i)
        {
            bytes_.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
        }
        return *this;
    }

    Writer&
    fixed32(int32_t field, uint32_t value)
    {
        tag(field, google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED32);
        for (uint32_t i{}; i < 4; ++i)
        {
            bytes_.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
        }
        return *this;
    }

    Writer&
    bytes(int32_t field, std::string const& value)
    {
        tag(field, google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
        raw(varintBytes(value.size()));
        raw(value);
        return *this;
    }

    Writer&
    group(int32_t field, std::string const& body)
    {
        tag(field, google::protobuf::internal::WireFormatLite::WIRETYPE_START_GROUP);
        raw(body);
        tag(field, google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP);
        return *this;
    }

    Writer&
    tag(int32_t field, google::protobuf::internal::WireFormatLite::WireType wireType)
    {
        raw(varintBytes(google::protobuf::internal::WireFormatLite::MakeTag(field, wireType)));
        return *this;
    }

    Writer&
    raw(std::string const& value)
    {
        bytes_ += value;
        return *this;
    }

    [[nodiscard]] std::string const&
    str() const
    {
        return bytes_;
    }

    [[nodiscard]] static std::string
    varintBytes(uint64_t value)
    {
        auto out = std::string{};
        do
        {
            auto byte = static_cast<uint8_t>(value & 0x7F);
            value >>= 7;
            if (value != 0)
            {
                byte |= 0x80;
            }
            out.push_back(static_cast<char>(byte));
        } while (value != 0);
        return out;
    }
};

[[nodiscard]] AdmissionDecision
scan(
    std::string const& bytes,
    BaseLimits const& limits = BaseLimits{},
    RepeatedCaps const& caps = {})
{
    auto const span =
        std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size()};
    return prescanProtobuf(span, *schema().test, limits, caps);
}

// A Sub nested `depth` levels deep, through `child`.
[[nodiscard]] std::string
nested(int32_t depth)
{
    auto body = Writer{}.varint(1, 7).str();
    for (int32_t i{1}; i < depth; ++i)
    {
        body = Writer{}.varint(1, 7).bytes(2, body).str();
    }
    return body;
}

// A generated-message stand-in for the runtime schema, with a spec whose check refuses an `a`
// over a tunable, and a cap of two `items`.
struct TestMessage
{
    static google::protobuf::Descriptor const*
    descriptor()
    {
        return schema().test;
    }
};

consteval auto
admissionSpec(std::type_identity<TestMessage>)
{
    return makeSpec<TestMessage>(
               tunable<"max_payload_bytes">(uint64_t{4096}, "admission.test.max_payload_bytes"),
               tunable<"size_ramp">(
                   ramp({{.upToBytes = 4096, .cost = 1.0}}), "admission.test.size_ramp"),
               tunable<"max_a">(uint64_t{100}, "admission.test.max_a"))
        .withAttributes(
            {.typeId = 1,
             .repeated = repeatedCaps({{.fieldNumber = 2, .maxEntries = 2, .minEntryBytes = 0}})})
        .withCheck([](VisitEvent const& event, auto const& cfg) -> AdmissionDecision {
            if (auto const* a = event.as<uint64_t>(); event.fieldNumber == 1 and
                event.depth == 0 and a != nullptr and *a > cfg.template get<"max_a">())
            {
                return AdmissionDecision::drop("a too large", PenaltyClass::Invalid);
            }
            return AdmissionDecision::admit();
        });
}

[[nodiscard]] std::span<uint8_t const>
spanOf(std::string const& bytes)
{
    return {reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size()};
}

}  // namespace

TEST(ProtobufPrescanTests, admits_a_well_formed_message_and_reports_its_values)
{
    auto const bytes = Writer{}
                           .varint(1, 5)
                           .bytes(2, "one")
                           .bytes(2, "two")
                           .bytes(3, nested(2))
                           .varint(4, 1)
                           .bytes(5, Writer::varintBytes(1) + Writer::varintBytes(300))
                           .fixed64(6, 9)
                           .str();
    ASSERT_TRUE(schema().parses(bytes));

    auto events = std::vector<EventKind>{};
    auto record = [&](VisitEvent const& event) {
        events.push_back(event.kind);
        return AdmissionDecision::admit();
    };
    auto const span =
        std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size()};
    EXPECT_TRUE(prescanProtobuf(span, *schema().test, BaseLimits{}, {}, record).admitted());

    auto const count = [&](EventKind kind) { return std::ranges::count(events, kind); };
    EXPECT_EQ(count(EventKind::Bytes), 2);
    EXPECT_EQ(count(EventKind::ObjectBegin), 2);
    EXPECT_EQ(count(EventKind::ObjectEnd), 2);
    // a, the two packed nums, and f.
    EXPECT_EQ(count(EventKind::Uint64), 4);
    // The two nested x, and color.
    EXPECT_EQ(count(EventKind::Int64), 3);
}

TEST(ProtobufPrescanTests, a_check_can_refuse_while_walking)
{
    auto const bytes = Writer{}.varint(1, 1000).str();
    auto refuseLarge = [](VisitEvent const& event) {
        auto const* value = event.as<uint64_t>();
        return value != nullptr and *value > 100
            ? AdmissionDecision::drop("a too large", PenaltyClass::Invalid)
            : AdmissionDecision::admit();
    };
    auto const span =
        std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size()};
    EXPECT_EQ(
        prescanProtobuf(span, *schema().test, BaseLimits{}, {}, refuseLarge).reason, "a too large");
}

TEST(ProtobufPrescanTests, refuses_input_no_conforming_encoder_produces)
{
    auto const malformed = [](std::string const& bytes) {
        auto const decision = scan(bytes);
        return decision.dropped() and decision.penalty == PenaltyClass::Malformed;
    };
    // Field number 0.
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(0, google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT)
            .raw("\x01")
            .str()));
    // Wire types 6 and 7.
    EXPECT_TRUE(malformed(Writer{}.raw(Writer::varintBytes((1 << 3) | 6)).raw("\x01").str()));
    EXPECT_TRUE(malformed(Writer{}.raw(Writer::varintBytes((1 << 3) | 7)).raw("\x01").str()));
    // A varint longer than 10 bytes.
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(1, google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT)
            .raw(std::string(11, '\xFF'))
            .raw("\x01")
            .str()));
    // A length larger than what is left, including one that would wrap a 64-bit offset.
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(2, google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED)
            .raw(Writer::varintBytes(100))
            .raw("ab")
            .str()));
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(2, google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED)
            .raw(Writer::varintBytes(std::numeric_limits<uint64_t>::max()))
            .str()));
    // A truncated fixed64.
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(6, google::protobuf::internal::WireFormatLite::WIRETYPE_FIXED64)
            .raw("1234")
            .str()));
}

// Cases the differential test found where CodedInputStream's readers differ from libprotobuf's
// parser. Each side of the comparison is checked against the parser itself.
TEST(ProtobufPrescanTests, reads_tags_as_libprotobufs_parser_does)
{
    auto const agrees = [](std::string const& bytes) {
        auto permissive = BaseLimits{};
        permissive.maxUnknownFields = std::numeric_limits<uint64_t>::max();
        permissive.maxUnknownFieldBytes = std::numeric_limits<uint64_t>::max();
        return scan(bytes, permissive).admitted() == schema().parses(bytes);
    };

    // A truncated tag at the end of the payload.
    auto const truncatedTag = std::string{"\xD3"};
    EXPECT_FALSE(schema().parses(truncatedTag));
    EXPECT_TRUE(agrees(truncatedTag));

    // A zero byte after the last field: tag 0 never ends a message.
    auto const trailingZero = Writer{}.varint(1, 5).raw(std::string(1, '\0')).str();
    EXPECT_FALSE(schema().parses(trailingZero));
    EXPECT_TRUE(agrees(trailingZero));

    // A 6-byte tag is refused.
    auto const sixByteTag = std::string{"\xE5\xCA\x96\x96\xAE\x1A\xBA\xD2\x10q"};
    EXPECT_FALSE(schema().parses(sixByteTag));
    EXPECT_TRUE(agrees(sixByteTag));

    // A 5-byte tag with bits above 32 keeps its low 32 bits.
    auto const highBits = Writer{}.varint(1, 5).raw("\xF0\xF0\xF0\x88\x14").raw("[").str();
    EXPECT_TRUE(schema().parses(highBits));
    EXPECT_TRUE(agrees(highBits));
}

TEST(ProtobufPrescanTests, limits_unknown_fields_by_count_and_bytes)
{
    auto limits = BaseLimits{};
    limits.maxUnknownFields = 2;
    limits.maxUnknownFieldBytes = 64;

    EXPECT_TRUE(scan(Writer{}.varint(40, 1).varint(41, 1).str(), limits).admitted());
    auto const tooMany = scan(Writer{}.varint(40, 1).varint(41, 1).varint(42, 1).str(), limits);
    EXPECT_EQ(tooMany.penalty, PenaltyClass::Invalid);

    // One unknown field large enough to pass the count but not the bytes.
    auto const tooLarge = scan(Writer{}.bytes(40, std::string(100, 'u')).str(), limits);
    EXPECT_EQ(tooLarge.penalty, PenaltyClass::Invalid);

    // A known field sent with the wrong wire type is unknown to libprotobuf too.
    limits.maxUnknownFields = 0;
    EXPECT_TRUE(scan(Writer{}.bytes(1, "x").str(), limits).dropped());
}

TEST(ProtobufPrescanTests, counts_an_undefined_closed_enum_value_as_unknown)
{
    auto limits = BaseLimits{};
    limits.maxUnknownFields = 0;
    EXPECT_TRUE(scan(Writer{}.varint(4, 1).str(), limits).admitted());
    EXPECT_EQ(scan(Writer{}.varint(4, 9).str(), limits).penalty, PenaltyClass::Invalid);
}

TEST(ProtobufPrescanTests, refuses_groups_unless_allowed)
{
    auto const bytes = Writer{}.group(40, Writer{}.varint(1, 1).str()).str();
    EXPECT_EQ(scan(bytes).penalty, PenaltyClass::Invalid);

    auto limits = BaseLimits{};
    limits.rejectGroups = false;
    EXPECT_TRUE(scan(bytes, limits).admitted());
}

TEST(ProtobufPrescanTests, limits_nesting_depth)
{
    auto limits = BaseLimits{};
    limits.maxDepth = 3;
    EXPECT_TRUE(scan(Writer{}.bytes(3, nested(3)).str(), limits).admitted());
    EXPECT_EQ(scan(Writer{}.bytes(3, nested(4)).str(), limits).penalty, PenaltyClass::Invalid);
}

TEST(ProtobufPrescanTests, caps_top_level_repeated_fields_packed_or_not)
{
    constexpr auto kCaps = repeatedCaps(
        {{.fieldNumber = 2, .maxEntries = 2, .minEntryBytes = 2},
         {.fieldNumber = 5, .maxEntries = 3, .minEntryBytes = 1}});

    EXPECT_TRUE(scan(Writer{}.bytes(2, "ab").bytes(2, "cd").str(), BaseLimits{}, kCaps).admitted());
    EXPECT_EQ(
        scan(Writer{}.bytes(2, "ab").bytes(2, "cd").bytes(2, "ef").str(), BaseLimits{}, kCaps)
            .penalty,
        PenaltyClass::Invalid);
    // An entry smaller than the cap allows: a one-byte string is three bytes with its length.
    // The cap counts the value, so set the minimum above it.
    constexpr auto kLarge = repeatedCaps({{.fieldNumber = 2, .maxEntries = 9, .minEntryBytes = 4}});
    EXPECT_EQ(
        scan(Writer{}.bytes(2, "a").str(), BaseLimits{}, kLarge).penalty, PenaltyClass::Invalid);

    // Packed elements count one by one, and unpacked ones add to them.
    auto const packed = Writer::varintBytes(1) + Writer::varintBytes(2) + Writer::varintBytes(3);
    EXPECT_TRUE(scan(Writer{}.bytes(5, packed).str(), BaseLimits{}, kCaps).admitted());
    EXPECT_EQ(
        scan(Writer{}.bytes(5, packed).varint(5, 4).str(), BaseLimits{}, kCaps).penalty,
        PenaltyClass::Invalid);
}

// The prescan must accept exactly what libprotobuf's parser accepts, when its own limits are out
// of the way: any difference lets a payload mean one thing to admission and another to the parse.
TEST(ProtobufPrescanTests, agrees_with_libprotobuf_on_mutated_payloads)
{
    auto const permissive = BaseLimits{
        .maxUnknownFields = std::numeric_limits<uint64_t>::max(),
        .maxUnknownFieldBytes = std::numeric_limits<uint64_t>::max(),
        .maxDepth = 64,
        .rejectGroups = false};

    auto rng = std::mt19937{20261106};
    auto const pick = [&](size_t n) {
        return std::uniform_int_distribution<size_t>{0, n - 1}(rng);
    };

    auto const seeds = std::vector<std::string>{
        Writer{}.varint(1, 5).bytes(2, "one").bytes(3, nested(3)).varint(4, 1).str(),
        Writer{}.bytes(5, Writer::varintBytes(1) + Writer::varintBytes(300)).fixed64(6, 9).str(),
        Writer{}.group(40, Writer{}.varint(1, 1).bytes(2, "g").str()).varint(41, 3).str(),
        Writer{}.bytes(3, Writer{}.bytes(2, nested(2)).varint(9, 1).str()).varint(4, 7).str(),
        std::string{},
    };

    auto disagreements = 0;
    for (int32_t i{}; i < 20000; ++i)
    {
        auto bytes = seeds[pick(seeds.size())];
        for (size_t m{}, n{1 + pick(4)}; m < n; ++m)
        {
            switch (pick(5))
            {
                case 0:  // flip a byte
                    if (not bytes.empty())
                    {
                        bytes[pick(bytes.size())] ^= static_cast<char>(1 + pick(255));
                    }
                    break;
                case 1:  // insert a byte
                    bytes.insert(
                        bytes.begin() + static_cast<std::ptrdiff_t>(pick(bytes.size() + 1)),
                        static_cast<char>(pick(256)));
                    break;
                case 2:  // truncate
                    bytes.resize(pick(bytes.size() + 1));
                    break;
                case 3:  // append a random field
                    bytes += Writer{}
                                 .raw(Writer::varintBytes(pick(1 << 12)))
                                 .raw(std::string(pick(4), static_cast<char>(pick(256))))
                                 .str();
                    break;
                default:  // splice in another seed
                    bytes.insert(pick(bytes.size() + 1), seeds[pick(seeds.size())]);
                    break;
            }
        }
        auto const prescanned = scan(bytes, permissive).admitted();
        auto const parsed = schema().parses(bytes);
        if (prescanned != parsed)
        {
            ++disagreements;
            ADD_FAILURE() << "prescan " << (prescanned ? "admits" : "refuses")
                          << " what libprotobuf " << (parsed ? "parses" : "rejects") << ": "
                          << testing::PrintToString(
                                 std::vector<uint8_t>(bytes.begin(), bytes.end()));
            if (disagreements >= 5)
            {
                break;
            }
        }
    }
    EXPECT_EQ(disagreements, 0);
}

TEST(ProtobufPrescanTests, reports_values_typed_by_their_declared_type)
{
    auto const bytes =
        Writer{}
            .varint(7, static_cast<uint64_t>(int64_t{-5}))
            .varint(8, google::protobuf::internal::WireFormatLite::ZigZagEncode64(-3))
            .varint(9, 1)
            .fixed64(10, std::bit_cast<uint64_t>(1.5))
            .bytes(11, "abc")
            .fixed32(12, std::bit_cast<uint32_t>(2.5F))
            .bytes(2, "raw")
            .bytes(3, nested(2))
            .str();
    ASSERT_TRUE(schema().parses(bytes));

    auto events = std::vector<VisitEvent>{};
    auto record = [&](VisitEvent const& event) {
        events.push_back(event);
        return AdmissionDecision::admit();
    };
    ASSERT_TRUE(
        prescanProtobuf(spanOf(bytes), *schema().test, BaseLimits{}, {}, record).admitted());

    auto const find = [&](uint64_t field, uint32_t depth = 0) {
        return *std::ranges::find_if(events, [&](VisitEvent const& e) {
            return e.fieldNumber == field and e.depth == depth;
        });
    };
    EXPECT_EQ(find(7).kind, EventKind::Int64);
    EXPECT_EQ(*find(7).as<int64_t>(), -5);
    EXPECT_EQ(*find(8).as<int64_t>(), -3);
    EXPECT_EQ(find(9).kind, EventKind::Bool);
    EXPECT_TRUE(*find(9).as<bool>());
    EXPECT_DOUBLE_EQ(*find(10).as<double>(), 1.5);
    EXPECT_EQ(find(11).kind, EventKind::String);
    EXPECT_EQ(*find(11).as<std::string_view>(), "abc");
    EXPECT_EQ(find(11).size, 3U);
    EXPECT_DOUBLE_EQ(*find(12).as<double>(), 2.5);
    EXPECT_EQ(find(2).kind, EventKind::Bytes);

    // The nested message: begin and end at depth 0, its fields at 1 and 2.
    EXPECT_EQ(find(3).kind, EventKind::ObjectBegin);
    auto const end = *std::ranges::find_if(events, [](VisitEvent const& e) {
        return e.kind == EventKind::ObjectEnd and e.depth == 0;
    });
    EXPECT_EQ(end.fieldNumber, 3U);
    EXPECT_EQ(end.size, 2U);  // x and child
    EXPECT_EQ(*find(1, 1).as<int64_t>(), 7);
    EXPECT_EQ(*find(1, 2).as<int64_t>(), 7);
}

TEST(ProtobufPrescanTests, runs_a_specs_check_and_caps)
{
    EXPECT_TRUE(prescan<TestMessage>(spanOf(Writer{}.varint(1, 100).str())).admitted());
    EXPECT_EQ(prescan<TestMessage>(spanOf(Writer{}.varint(1, 101).str())).reason, "a too large");
    // The check reads the top level only: an `x` of 101 in a nested message is admitted.
    EXPECT_TRUE(
        prescan<TestMessage>(spanOf(Writer{}.bytes(3, Writer{}.varint(1, 101).str()).str()))
            .admitted());
    // The spec's cap of two items.
    auto const three = Writer{}.bytes(2, "a").bytes(2, "b").bytes(2, "c").str();
    EXPECT_EQ(prescan<TestMessage>(spanOf(three)).penalty, PenaltyClass::Invalid);
}

TEST(ProtobufPrescanTests, runs_as_the_body_stage)
{
    auto limiter = ConnectionLimiter<int32_t>{
        BucketSettings{.capacity = 100.0, .refillRatePerSecond = 1.0}, 4};
    auto const now = std::chrono::steady_clock::now();
    auto const costs = PenaltyCosts{.trivial = 1, .useless = 5, .invalid = 10, .malformed = 20};
    auto sink = limiter.sink(1, now, costs);

    auto const good = Writer{}.varint(1, 5).str();
    auto const bad = Writer{}.varint(1, 500).str();
    EXPECT_TRUE(
        admitBody<TestMessage>(protobufVisitor<TestMessage>(spanOf(good)), sink)
            .decision.admitted());
    auto const refused = admitBody<TestMessage>(protobufVisitor<TestMessage>(spanOf(bad)), sink);
    EXPECT_EQ(refused.decision.reason, "a too large");
    EXPECT_EQ(refused.decision.penalty, PenaltyClass::Invalid);
}
