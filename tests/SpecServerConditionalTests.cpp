// Clio-backend tests: this translation unit is the only one in its executable
// (rpcspec_clio_tests), compiled with RPCSPEC_IS_CLIO=1. The two server-backend
// macros are mutually exclusive within a binary, so this is the ONLY place the
// Clio branch of ifServerClio() / ifServerXrpld() can be exercised. The
// complementary xrpld branch is covered by RpcSpecDSL_ServerConditional in
// SpecValidatorTests.cpp.

#include <boost/json/parse.hpp>

#include <gtest/gtest.h>
#include <rpcspec/Aliases.hpp>
#include <rpcspec/Errors.hpp>
#include <rpcspec/FieldSpec.hpp>
#include <rpcspec/RpcSpec.hpp>
#include <rpcspec/SpecDumpWriter.hpp>
#include <rpcspec/handlers/subscribe/Spec.hpp>
#include <rpcspec/handlers/subscribe/Types.hpp>
#include <rpcspec/handlers/unsubscribe/Spec.hpp>
#include <rpcspec/handlers/unsubscribe/Types.hpp>

#include <sstream>
#include <string>

using namespace rpc::spec;

namespace {
// `clio_only` rejects a true value only in Clio builds; `xrpld_only` rejects a
// true value only in xrpld builds. Under RPCSPEC_IS_CLIO the first fires and
// the second is inert.
constexpr auto kSPEC = RpcSpec{
    field("clio_only", ifServerClio(notSupportedIf(true))),
    field("xrpld_only", ifServerXrpld(notSupportedIf(true))),
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

TEST(ServerConditionalClio, IfServerXrpldValidatorIsInertInClioBuild)
{
    auto value = boost::json::parse(R"JSON({ "xrpld_only": true })JSON");
    EXPECT_TRUE(kSPEC.process(value).has_value());
}

TEST(SubscribeSpecClio, ServerStreamRejectedWithNotSupported)
{
    auto value = boost::json::parse(R"JSON({"streams": ["server"]})JSON");
    auto const r = handlers::subscribe::kInputSpec.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcNotSupported);
}

TEST(SubscribeSpecClio, ConsensusStreamRejectedWithNotSupported)
{
    auto value = boost::json::parse(R"JSON({"streams": ["consensus"]})JSON");
    auto const r = handlers::subscribe::kInputSpec.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcNotSupported);
}

TEST(SubscribeSpecClio, PeerStatusStreamRejectedWithNotSupported)
{
    auto value = boost::json::parse(R"JSON({"streams": ["peer_status"]})JSON");
    auto const r = handlers::subscribe::kInputSpec.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcNotSupported);
}

TEST(SubscribeSpecClio, LedgerStreamAccepted)
{
    auto value = boost::json::parse(R"JSON({"streams": ["ledger"]})JSON");
    auto const r = handlers::subscribe::kInputSpec.parse(value);
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->streams.has_value());
    ASSERT_EQ(r->streams->size(), 1u);
    EXPECT_EQ((*r->streams)[0], handlers::subscribe::StreamType::Ledger);
}

TEST(SubscribeDumpClio, DumpContainsNotSupportedAndServerStream)
{
    std::ostringstream oss;
    rpc::spec::SpecDumpWriter w{oss};
    handlers::subscribe::kInputSpec.dump(w);
    auto const s = oss.str();

    static constexpr auto npos = std::string::npos;
    EXPECT_NE(s.find("notSupported"), npos) << "missing: notSupported";
    EXPECT_NE(s.find("server"), npos) << "missing: server";
}

TEST(UnsubscribeSpecClio, ServerStreamRejectedWithNotSupported)
{
    auto value = boost::json::parse(R"JSON({"streams": ["server"]})JSON");
    auto const r = handlers::unsubscribe::kInputSpec.parse(value);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), rpc::RippledError::RpcNotSupported);
}

TEST(UnsubscribeSpecClio, LedgerStreamAccepted)
{
    auto value = boost::json::parse(R"JSON({"streams": ["ledger"]})JSON");
    auto const r = handlers::unsubscribe::kInputSpec.parse(value);
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->streams.has_value());
    ASSERT_EQ(r->streams->size(), 1u);
    EXPECT_EQ((*r->streams)[0], handlers::unsubscribe::StreamType::Ledger);
}
