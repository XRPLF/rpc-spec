// Clio-backend tests: this translation unit is the only one in its executable
// (rpcspec_clio_tests), compiled with RPCSPEC_IS_CLIO=1. The two server-backend
// macros are mutually exclusive within a binary, so this is the ONLY place the
// Clio branch of ifServerClio() / ifServerRippled() can be exercised. The
// complementary rippled branch is covered by RpcSpecDSL_ServerConditional in
// SpecValidatorTests.cpp.

#include <rpcspec/Aliases.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/RpcSpec.hpp>

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>

using namespace rpc::spec;

namespace {
// `clio_only` rejects a true value only in Clio builds; `rippled_only` rejects a
// true value only in rippled builds. Under RPCSPEC_IS_CLIO the first fires and
// the second is inert.
constexpr auto kSPEC = RpcSpec{
    field("clio_only", ifServerClio(notSupportedIf(true))),
    field("rippled_only", ifServerRippled(notSupportedIf(true))),
};
}  // namespace

TEST(ServerConditionalClio, IfServerClioValidatorIsApplied)
{
    auto bad = boost::json::parse(R"JSON({ "clio_only": true })JSON");
    auto const r = kSPEC.process(bad);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcNotSupported);
}

TEST(ServerConditionalClio, IfServerClioValidatorAllowsNonTriggeringValue)
{
    auto ok = boost::json::parse(R"JSON({ "clio_only": false })JSON");
    EXPECT_TRUE(kSPEC.process(ok).has_value());
}

TEST(ServerConditionalClio, IfServerRippledValidatorIsInertInClioBuild)
{
    auto value = boost::json::parse(R"JSON({ "rippled_only": true })JSON");
    EXPECT_TRUE(kSPEC.process(value).has_value());
}
