#include <admissionspec/AdmissionDecision.hpp>
#include <admissionspec/JsonVisitor.hpp>
#include <admissionspec/PenaltyClass.hpp>
#include <admissionspec/RepeatedCaps.hpp>
#include <admissionspec/RepeatedCapsCheck.hpp>
#include <admissionspec/Rule.hpp>
#include <admissionspec/VisitEvent.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string_view>

using namespace admission::spec;

namespace {

[[nodiscard]] AdmissionDecision
admitAll(VisitEvent const&)
{
    return AdmissionDecision::admit();
}

[[nodiscard]] AdmissionDecision
walkJson(std::string_view json, RepeatedCaps const& caps)
{
    auto check = RepeatedCapsCheck{caps, admitAll};
    return visitJson(
        std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(json.data()), json.size()},
        check);
}

}  // namespace

TEST(RepeatedCapsCheckTests, counts_protobuf_entries_at_the_top_level_by_field_number)
{
    constexpr auto kCaps = repeatedCaps({{.fieldNumber = 2, .maxEntries = 2, .minEntryBytes = 2}});
    auto check = RepeatedCapsCheck{kCaps, admitAll};
    auto const entry = [](uint32_t depth, uint64_t size) {
        return VisitEvent{.kind = EventKind::Bytes, .size = size, .depth = depth, .fieldNumber = 2};
    };

    EXPECT_TRUE(check(entry(0, 2)).admitted());
    // The same field number in a nested message is not counted.
    EXPECT_TRUE(check(entry(1, 2)).admitted());
    EXPECT_TRUE(check(entry(0, 2)).admitted());
    auto const over = check(entry(0, 2));
    EXPECT_EQ(over.penalty, PenaltyClass::Invalid);
    EXPECT_EQ(over.rule, Rule::RepeatedCap);

    auto small = RepeatedCapsCheck{kCaps, admitAll};
    auto const tooSmall = small(entry(0, 1));
    EXPECT_EQ(tooSmall.reason, "repeated entry smaller than its type allows");
    EXPECT_EQ(tooSmall.rule, Rule::EntrySize);
}

TEST(RepeatedCapsCheckTests, counts_the_elements_of_a_json_array_by_key)
{
    constexpr auto kCaps = repeatedCaps({{.key = "items", .maxEntries = 3, .minEntryBytes = 2}});

    EXPECT_TRUE(walkJson(R"({"items": ["ab", "cd", "ef"]})", kCaps).admitted());
    EXPECT_EQ(
        walkJson(R"({"items": ["ab", "cd", "ef", "gh"]})", kCaps).reason,
        "too many entries in a repeated field");
    EXPECT_EQ(
        walkJson(R"({"items": ["a"]})", kCaps).reason,
        "repeated entry smaller than its type allows");

    // Elements of a nested array, or objects, count once each.
    EXPECT_TRUE(walkJson(R"({"items": [[1, 2, 3, 4], {"x": 1}, 7]})", kCaps).admitted());
    EXPECT_TRUE(walkJson(R"({"items": [1, 2, 3], "other": [1, 2, 3, 4]})", kCaps).admitted());

    // The array can be anywhere, and each array with the key is counted on its own.
    EXPECT_TRUE(
        walkJson(R"({"params": [{"items": [1, 2, 3]}, {"items": [1, 2, 3]}]})", kCaps).admitted());
    EXPECT_FALSE(walkJson(R"({"params": [{"items": [1, 2, 3, 4]}]})", kCaps).admitted());
}

TEST(RepeatedCapsCheckTests, hands_every_event_to_the_specs_check)
{
    constexpr auto kCaps = repeatedCaps({{.key = "items", .maxEntries = 9}});
    auto seen = 0;
    auto check = RepeatedCapsCheck{kCaps, [&](VisitEvent const&) {
                                       ++seen;
                                       return AdmissionDecision::admit();
                                   }};
    auto const json = std::string_view{R"({"items": [1, 2]})"};
    EXPECT_TRUE(
        visitJson(
            std::span<uint8_t const>{reinterpret_cast<uint8_t const*>(json.data()), json.size()},
            check)
            .admitted());
    EXPECT_GT(seen, 4);
}
