/**
 * @file
 *  GTest coverage for the backend-independent core types: Status, typeNameOf<T>, and the
 *  ledger-type registry. Deliberately touches none of the validator/spec machinery, so it
 *  compiles identically under either server backend.
 */
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

static_assert(rpc::spec::typeNameOf<int64_t>() == "int64");
static_assert(rpc::spec::typeNameOf<bool>() == "bool");
static_assert(rpc::spec::typeNameOf<std::string>() == "string");

TEST(RpcSpec, StatusDefault)
{
    rpc::Status const status;
    EXPECT_FALSE(static_cast<bool>(status));
    EXPECT_TRUE(status == xrpl::RpcSuccess);
}

TEST(RpcSpec, LedgerTypesTable)
{
    constexpr auto& table = rpc::spec::kLedgerTypesTable;
    static_assert(not table.empty());

    auto const it = std::ranges::find_if(
        table, [](auto const& fieldView) { return fieldView.rpcName == "account"; });
    ASSERT_NE(it, table.end());
    EXPECT_EQ(it->type, xrpl::ltACCOUNT_ROOT);
}

TEST(RpcSpec, SponsorshipIsARegisteredDeletionBlocker)
{
    constexpr auto& table = rpc::spec::kLedgerTypesTable;

    auto const it = std::ranges::find_if(
        table, [](auto const& fieldView) { return fieldView.rpcName == "sponsorship"; });
    ASSERT_NE(it, table.end());
    EXPECT_EQ(it->type, xrpl::ltSPONSORSHIP);
    // ltSPONSORSHIP is not in AccountDelete's nonObligationDeleter allowlist, so an
    // owned Sponsorship blocks account deletion.
    EXPECT_EQ(it->category, rpc::spec::LedgerCategory::DeletionBlocker);
}

TEST(RpcSpec, DeletionBlockersPresent)
{
    constexpr auto& table = rpc::spec::kLedgerTypesTable;

    auto count = std::ranges::count_if(table, [](auto const& fieldView) {
        return fieldView.category == rpc::spec::LedgerCategory::DeletionBlocker;
    });
    EXPECT_GT(count, 0);
}

}  // namespace
