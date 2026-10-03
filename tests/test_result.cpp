#include <gtest/gtest.h>

#include <string>

#include "core/error.hpp"

using mc::Error;
using mc::Result;

TEST(ResultTest, HoldsValueOnSuccess) {
    Result<int> r(42);
    EXPECT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(r.value(), 42);
    EXPECT_EQ(*r, 42);
}

TEST(ResultTest, HoldsErrorOnFailure) {
    Result<int> r(Error("boom"));
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.error().message, "boom");
}

TEST(ResultTest, MakeErrorTagsLocation) {
    const Error e = mc::makeError("something broke", __FILE__, __LINE__ - 1);
    ASSERT_FALSE(e.location.empty());
    EXPECT_NE(e.describe().find("something broke"), std::string::npos);
    EXPECT_NE(e.describe().find("test_result.cpp"), std::string::npos);
}

TEST(ResultTest, VoidResultOkByDefault) {
    Result<void> r;
    EXPECT_TRUE(r.ok());
}

TEST(ResultTest, VoidResultFromError) {
    Result<void> r(Error("failed"));
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.error().message, "failed");
}

namespace {

Result<int> add(int a, int b) {
    if (a < 0 || b < 0) {
        return MC_FAIL("negative input");
    }
    return a + b;
}

Result<int> chained(int a) {
    auto inner = add(a, 1);
    MC_TRY(inner);
    return *inner + 100;
}

Result<void> voidChain(bool okFlag) {
    if (!okFlag) {
        return MC_FAIL("flag was false");
    }
    auto x = add(1, 1);
    MC_TRY(x);
    return {};
}

} // namespace

TEST(McTryTest, PropagatesError) {
    const auto r = add(-1, 2);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().describe().find("negative input"), std::string::npos);
    EXPECT_NE(r.error().describe().find("test_result.cpp"), std::string::npos);
}

TEST(McTryTest, ValuePath) {
    const auto r = add(2, 3);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(*r, 5);
}

TEST(McTryTest, ChainedPropagation) {
    EXPECT_FALSE(chained(-5).ok());
    const auto r = chained(4);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(*r, 105);
}

TEST(McTryTest, VoidPropagation) {
    EXPECT_TRUE(voidChain(true).ok());
    const auto r = voidChain(false);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().message, "flag was false");
}
