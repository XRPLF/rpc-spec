#include <boost/json/parse.hpp>

#include <gtest/gtest.h>

#include <Backend.hpp>

using namespace rpc::spec;

namespace {

// FieldView / ObjectView navigation — child() and element() on the boost::json backend.

TEST(RpcSpecDSL_FieldView, ChildReturnsAbsentFaWhenParentAbsent)
{
    auto request = boost::json::parse(R"JSON({})JSON");
    ObjectView root{request};
    auto fieldView = root.child("foo");
    EXPECT_FALSE(fieldView.present());
    auto child = fieldView.child("bar");
    EXPECT_FALSE(child.present());
}

TEST(RpcSpecDSL_FieldView, ChildReturnsAbsentFaWhenParentNotObject)
{
    auto request = boost::json::parse(R"JSON({ "foo": 42 })JSON");
    ObjectView root{request};
    auto fieldView = root.child("foo");
    EXPECT_TRUE(fieldView.present());
    EXPECT_FALSE(fieldView.isObject());
    auto child = fieldView.child("bar");
    EXPECT_FALSE(child.present());
}

TEST(RpcSpecDSL_FieldView, ChildNavigatesIntoSubObject)
{
    auto request = boost::json::parse(R"JSON({ "foo": { "bar": "hello" } })JSON");
    ObjectView root{request};
    auto fieldView = root.child("foo");
    ASSERT_TRUE(fieldView.present());
    ASSERT_TRUE(fieldView.isObject());

    auto child = fieldView.child("bar");
    ASSERT_TRUE(child.present());
    EXPECT_TRUE(child.isString());
    EXPECT_EQ(child.asString(), "hello");
}

TEST(RpcSpecDSL_FieldView, ChildMissingKeyReturnsAbsent)
{
    auto request = boost::json::parse(R"JSON({ "foo": { "a": 1 } })JSON");
    ObjectView root{request};
    auto fieldView = root.child("foo");
    auto child = fieldView.child("missing");
    EXPECT_FALSE(child.present());
}

TEST(RpcSpecDSL_FieldView, ElementNavigatesIntoArray)
{
    auto request = boost::json::parse(R"JSON({ "ids": [10, 20, 30] })JSON");
    ObjectView root{request};
    auto fieldView = root.child("ids");
    ASSERT_TRUE(fieldView.isArray());

    auto elem0 = fieldView.element(0);
    ASSERT_TRUE(elem0.present());
    EXPECT_TRUE(elem0.isInt64());
    EXPECT_EQ(elem0.asInt64(), 10);

    auto elem2 = fieldView.element(2);
    ASSERT_TRUE(elem2.present());
    EXPECT_EQ(elem2.asInt64(), 30);
}

TEST(RpcSpecDSL_FieldView, ElementOutOfBoundsReturnsAbsent)
{
    auto request = boost::json::parse(R"JSON({ "ids": [1, 2] })JSON");
    ObjectView root{request};
    auto fieldView = root.child("ids");
    EXPECT_FALSE(fieldView.element(5).present());
}

TEST(RpcSpecDSL_FieldView, RootOverNonObjectReportsIsObjectFalse)
{
    auto arr = boost::json::parse(R"JSON([1, 2, 3])JSON");
    ObjectView root{arr};
    EXPECT_FALSE(root.isObject());
    EXPECT_TRUE(root.isArray());
    auto fieldView = root.child("anything");
    EXPECT_FALSE(fieldView.present());
}

TEST(RpcSpecDSL_FieldView, ConstRootYieldsReadOnlyChild)
{
    auto const request = boost::json::parse(R"JSON({ "foo": 1 })JSON");
    ObjectView const root{request};
    auto fieldView = root.child("foo");
    ASSERT_TRUE(fieldView.present());
    EXPECT_TRUE(fieldView.isInt64());
    EXPECT_EQ(fieldView.asInt64(), 1);
}

TEST(RpcSpecDSL_FieldView, ObjectSizeReportsMemberCount)
{
    auto request = boost::json::parse(R"JSON({ "empty": {}, "one": {"a": 1}, "arr": [1, 2] })JSON");
    ObjectView root{request};
    EXPECT_EQ(root.child("empty").objectSize(), 0);
    EXPECT_EQ(root.child("one").objectSize(), 1);
    // non-objects report 0 rather than the array/scalar size
    EXPECT_EQ(root.child("arr").objectSize(), 0);
    EXPECT_EQ(root.child("missing").objectSize(), 0);
}

}  // namespace
