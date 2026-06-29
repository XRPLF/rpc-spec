#include <admissionspec/Resolver.hpp>
#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <vector>

class Config
{
public:
    using ConfigValue =
        std::variant<std::uint64_t, double, std::string, std::vector<std::pair<uint64_t, double>>>;

    template <typename T>
    [[nodiscard]] std::optional<T>
    maybeValue(std::string_view key) const
    {
        auto it = values.find(std::string{key});
        if (it != std::end(values))
        {
            if (auto const* v = std::get_if<T>(&it->second))
            {
                return *v;
            }
        }
        return std::nullopt;
    }

    std::unordered_map<std::string, ConfigValue> values;
};

TEST(ResolverTests, Foo)
{
    auto config = Config{};
    config.values["foo.max_payload_bytes"] = std::uint64_t{1024};
    config.values["foo.size_ramp"] =
        std::vector<std::pair<uint64_t, double>>{{512, 5.0}, {1024, 10.0}};
    config.values["foo.bar"] = std::string{"hello"};

    constexpr auto t1 =
        admission::spec::tunable<"foo.max_payload_bytes">(2048ull, "foo.max_payload_bytes");
    constexpr auto t2 = admission::spec::tunable<"foo.size_ramp">(
        admission::spec::ramp({{256, 2.5}, {512, 5.0}}), "foo.size_ramp");
    constexpr auto t3 = admission::spec::tunable<"foo.bar">(std::string{"default"}, "foo.bar");
    constexpr auto t4 = admission::spec::tunable<"foo.baz">(std::string{"default"}, "foo.baz");

    constexpr auto tunables = std::make_tuple(t1, t2, t3, t4);

    auto resolved = admission::spec::detail::resolveTuple(tunables, config);

    EXPECT_EQ(resolved.template get<"foo.max_payload_bytes">(), std::uint64_t{1024});
    auto expectedRamp = std::vector<admission::spec::SizeTier>{{512, 5.0}, {1024, 10.0}};
    EXPECT_EQ(resolved.template get<"foo.size_ramp">(), expectedRamp);
    EXPECT_EQ(resolved.template get<"foo.bar">(), std::string{"hello"});
    EXPECT_EQ(resolved.template get<"foo.baz">(), std::string{"default"});
}
