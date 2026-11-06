#include <admissionspec/ProtobufPrescan.hpp>
#include <admissionspec/SpecAttributes.hpp>
#include <admissionspec/Types.hpp>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message.h>
#include <google/protobuf/wire_format_lite.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <vector>

using namespace admission::spec;
namespace gpb = google::protobuf;
using WireFormat = gpb::internal::WireFormatLite;

namespace {

// enum Color { RED = 0; GREEN = 1; }
// message Sub { optional int32 x = 1; optional Sub child = 2; }
// message Test {
//   optional uint32 a = 1; repeated bytes items = 2; optional Sub sub = 3;
//   optional Color color = 4; repeated uint32 nums = 5 [packed = true]; optional fixed64 f = 6;
// }
class Schema
{
    gpb::DescriptorPool pool_;
    gpb::DynamicMessageFactory factory_{&pool_};

public:
    gpb::Descriptor const* test{};

    Schema()
    {
        auto file = gpb::FileDescriptorProto{};
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

        auto const add = [](gpb::DescriptorProto& message,
                            std::string const& name,
                            int number,
                            gpb::FieldDescriptorProto::Type type,
                            gpb::FieldDescriptorProto::Label label,
                            std::string const& typeName = {}) -> gpb::FieldDescriptorProto& {
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
        using F = gpb::FieldDescriptorProto;

        auto& sub = *file.add_message_type();
        sub.set_name("Sub");
        add(sub, "x", 1, F::TYPE_INT32, F::LABEL_OPTIONAL);
        add(sub, "child", 2, F::TYPE_MESSAGE, F::LABEL_OPTIONAL, ".prescan.Sub");

        auto& test = *file.add_message_type();
        test.set_name("Test");
        add(test, "a", 1, F::TYPE_UINT32, F::LABEL_OPTIONAL);
        add(test, "items", 2, F::TYPE_BYTES, F::LABEL_REPEATED);
        add(test, "sub", 3, F::TYPE_MESSAGE, F::LABEL_OPTIONAL, ".prescan.Sub");
        add(test, "color", 4, F::TYPE_ENUM, F::LABEL_OPTIONAL, ".prescan.Color");
        add(test, "nums", 5, F::TYPE_UINT32, F::LABEL_REPEATED).mutable_options()->set_packed(true);
        add(test, "f", 6, F::TYPE_FIXED64, F::LABEL_OPTIONAL);

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
        auto const message = std::unique_ptr<gpb::Message>{factory_.GetPrototype(test)->New()};
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
    varint(int field, uint64_t value)
    {
        tag(field, WireFormat::WIRETYPE_VARINT);
        raw(varintBytes(value));
        return *this;
    }

    Writer&
    fixed64(int field, uint64_t value)
    {
        tag(field, WireFormat::WIRETYPE_FIXED64);
        for (int i = 0; i < 8; ++i)
        {
            bytes_.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
        }
        return *this;
    }

    Writer&
    bytes(int field, std::string const& value)
    {
        tag(field, WireFormat::WIRETYPE_LENGTH_DELIMITED);
        raw(varintBytes(value.size()));
        raw(value);
        return *this;
    }

    Writer&
    group(int field, std::string const& body)
    {
        tag(field, WireFormat::WIRETYPE_START_GROUP);
        raw(body);
        tag(field, WireFormat::WIRETYPE_END_GROUP);
        return *this;
    }

    Writer&
    tag(int field, WireFormat::WireType wireType)
    {
        raw(varintBytes(WireFormat::MakeTag(field, wireType)));
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
    PrescanLimits const& limits = prescanLimits(),
    RepeatedCaps const& caps = {})
{
    auto const span =
        std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size()};
    return prescanProtobuf(span, *schema().test, limits, caps);
}

// A Sub nested `depth` levels deep, through `child`.
[[nodiscard]] std::string
nested(int depth)
{
    auto body = Writer{}.varint(1, 7).str();
    for (int i = 1; i < depth; ++i)
    {
        body = Writer{}.varint(1, 7).bytes(2, body).str();
    }
    return body;
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
    EXPECT_TRUE(prescanProtobuf(span, *schema().test, prescanLimits(), {}, record).admitted());

    auto const count = [&](EventKind kind) { return std::ranges::count(events, kind); };
    EXPECT_EQ(count(EventKind::Bytes), 2);
    EXPECT_EQ(count(EventKind::ObjectBegin), 2);
    EXPECT_EQ(count(EventKind::ObjectEnd), 2);
    // a, the two nested x, color, the two packed nums, and f.
    EXPECT_EQ(count(EventKind::Uint64), 7);
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
        prescanProtobuf(span, *schema().test, prescanLimits(), {}, refuseLarge).reason,
        "a too large");
}

TEST(ProtobufPrescanTests, refuses_input_no_conforming_encoder_produces)
{
    auto const malformed = [](std::string const& bytes) {
        auto const decision = scan(bytes);
        return decision.dropped() and decision.penalty == PenaltyClass::Malformed;
    };
    // Field number 0.
    EXPECT_TRUE(malformed(Writer{}.tag(0, WireFormat::WIRETYPE_VARINT).raw("\x01").str()));
    // Wire types 6 and 7.
    EXPECT_TRUE(malformed(Writer{}.raw(Writer::varintBytes((1 << 3) | 6)).raw("\x01").str()));
    EXPECT_TRUE(malformed(Writer{}.raw(Writer::varintBytes((1 << 3) | 7)).raw("\x01").str()));
    // A varint longer than 10 bytes.
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(1, WireFormat::WIRETYPE_VARINT)
            .raw(std::string(11, '\xFF'))
            .raw("\x01")
            .str()));
    // A length larger than what is left, including one that would wrap a 64-bit offset.
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(2, WireFormat::WIRETYPE_LENGTH_DELIMITED)
            .raw(Writer::varintBytes(100))
            .raw("ab")
            .str()));
    EXPECT_TRUE(malformed(
        Writer{}
            .tag(2, WireFormat::WIRETYPE_LENGTH_DELIMITED)
            .raw(Writer::varintBytes(std::numeric_limits<uint64_t>::max()))
            .str()));
    // A truncated fixed64.
    EXPECT_TRUE(malformed(Writer{}.tag(6, WireFormat::WIRETYPE_FIXED64).raw("1234").str()));
}

// Cases the differential test found where CodedInputStream's readers differ from libprotobuf's
// parser. Each side of the comparison is checked against the parser itself.
TEST(ProtobufPrescanTests, reads_tags_as_libprotobufs_parser_does)
{
    auto const agrees = [](std::string const& bytes) {
        auto permissive = prescanLimits();
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
    auto limits = prescanLimits();
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
    auto limits = prescanLimits();
    limits.maxUnknownFields = 0;
    EXPECT_TRUE(scan(Writer{}.varint(4, 1).str(), limits).admitted());
    EXPECT_EQ(scan(Writer{}.varint(4, 9).str(), limits).penalty, PenaltyClass::Invalid);
}

TEST(ProtobufPrescanTests, refuses_groups_unless_allowed)
{
    auto const bytes = Writer{}.group(40, Writer{}.varint(1, 1).str()).str();
    EXPECT_EQ(scan(bytes).penalty, PenaltyClass::Invalid);

    auto limits = prescanLimits();
    limits.rejectGroups = false;
    EXPECT_TRUE(scan(bytes, limits).admitted());
}

TEST(ProtobufPrescanTests, limits_nesting_depth)
{
    auto limits = prescanLimits();
    limits.maxDepth = 3;
    EXPECT_TRUE(scan(Writer{}.bytes(3, nested(3)).str(), limits).admitted());
    EXPECT_EQ(scan(Writer{}.bytes(3, nested(4)).str(), limits).penalty, PenaltyClass::Invalid);
}

TEST(ProtobufPrescanTests, caps_top_level_repeated_fields_packed_or_not)
{
    constexpr auto kCaps = repeatedCaps(
        {{.fieldNumber = 2, .maxEntries = 2, .minEntryBytes = 2},
         {.fieldNumber = 5, .maxEntries = 3, .minEntryBytes = 1}});

    EXPECT_TRUE(
        scan(Writer{}.bytes(2, "ab").bytes(2, "cd").str(), prescanLimits(), kCaps).admitted());
    EXPECT_EQ(
        scan(Writer{}.bytes(2, "ab").bytes(2, "cd").bytes(2, "ef").str(), prescanLimits(), kCaps)
            .penalty,
        PenaltyClass::Invalid);
    // An entry smaller than the cap allows: a one-byte string is three bytes with its length.
    // The cap counts the value, so set the minimum above it.
    constexpr auto kLarge = repeatedCaps({{.fieldNumber = 2, .maxEntries = 9, .minEntryBytes = 4}});
    EXPECT_EQ(
        scan(Writer{}.bytes(2, "a").str(), prescanLimits(), kLarge).penalty, PenaltyClass::Invalid);

    // Packed elements count one by one, and unpacked ones add to them.
    auto const packed = Writer::varintBytes(1) + Writer::varintBytes(2) + Writer::varintBytes(3);
    EXPECT_TRUE(scan(Writer{}.bytes(5, packed).str(), prescanLimits(), kCaps).admitted());
    EXPECT_EQ(
        scan(Writer{}.bytes(5, packed).varint(5, 4).str(), prescanLimits(), kCaps).penalty,
        PenaltyClass::Invalid);
}

// The prescan must accept exactly what libprotobuf's parser accepts, when its own limits are out
// of the way: any difference lets a payload mean one thing to admission and another to the parse.
TEST(ProtobufPrescanTests, agrees_with_libprotobuf_on_mutated_payloads)
{
    auto const permissive = PrescanLimits{
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
    for (int i = 0; i < 20000; ++i)
    {
        auto bytes = seeds[pick(seeds.size())];
        for (size_t m = 0, n = 1 + pick(4); m < n; ++m)
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
                        bytes.begin() + static_cast<long>(pick(bytes.size() + 1)),
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
