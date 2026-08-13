// Core spec DSL — compiles under both Clio (xrpl:: namespace) and xrpld (xrpl::).
// Validators.hpp is Clio-specific (JSON param validation) and not included here.
#include <gtest/gtest.h>
#include <rpcspec/Errors.hpp>
#include <rpcspec/LedgerTypes.hpp>
#include <rpcspec/Types.hpp>
#include <rpcspec/detail/XrplParse.hpp>

#include <xrpl_mock.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace {

// Compile-time checks that core DSL types are usable.
static_assert(rpc::spec::typeNameOf<int64_t>() == "int64");
static_assert(rpc::spec::typeNameOf<bool>() == "bool");
static_assert(rpc::spec::typeNameOf<std::string>() == "string");

TEST(RpcSpec, StatusDefault)
{
    rpc::Status const s;
    EXPECT_FALSE(static_cast<bool>(s));
    EXPECT_TRUE(s == xrpl::RpcSuccess);
}

TEST(RpcSpec, LedgerTypesTable)
{
    constexpr auto& table = rpc::spec::kLedgerTypesTable;
    static_assert(!table.empty());

    auto const it =
        std::ranges::find_if(table, [](auto const& e) { return e.rpcName == "account"; });
    ASSERT_NE(it, table.end());
    EXPECT_EQ(it->type, xrpl::ltACCOUNT_ROOT);
}

TEST(RpcSpec, DeletionBlockersPresent)
{
    constexpr auto& table = rpc::spec::kLedgerTypesTable;

    auto count = std::ranges::count_if(table, [](auto const& e) {
        return e.category == rpc::spec::LedgerCategory::DeletionBlocker;
    });
    EXPECT_GT(count, 0);
}

}  // namespace
