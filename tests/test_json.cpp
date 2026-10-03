#include <gtest/gtest.h>

#include <string>

#include "core/json.hpp"

using mc::Json;

namespace {

// Parses and asserts success; returns the value.
Json parse(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        // ASSERT_* cannot be used in a non-void function.
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json();
    }
    return std::move(*r);
}

void expectParseFail(const std::string& text) {
    const auto r = Json::parse(text);
    EXPECT_FALSE(r.ok()) << "expected failure for: " << text;
}

} // namespace

TEST(JsonParse, Primitives) {
    EXPECT_TRUE(parse("null").isNull());
    EXPECT_TRUE(parse("true").asBool());
    EXPECT_FALSE(parse("false").asBool());
    EXPECT_EQ(parse("42").asInt(), 42);
    EXPECT_EQ(parse("-7").asInt(), -7);
    EXPECT_DOUBLE_EQ(parse("3.25").asDouble(), 3.25);
    EXPECT_DOUBLE_EQ(parse("-1.5e2").asDouble(), -150.0);
    EXPECT_DOUBLE_EQ(parse("2.5E-3").asDouble(), 0.0025);
    EXPECT_EQ(parse("\"hello\"").asString(), "hello");
    EXPECT_EQ(parse("\"\"").asString(), "");
}

TEST(JsonParse, WhitespaceTolerated) {
    EXPECT_EQ(parse("  \n\t 42 \r\n ").asInt(), 42);
}

TEST(JsonParse, Objects) {
    const Json j = parse(R"({
        "window": { "width": 1280, "height": 720 },
        "vsync": true,
        "pi": 3.14
    })");
    ASSERT_TRUE(j.isObject());
    ASSERT_EQ(j.memberCount(), 3u);
    ASSERT_TRUE(j.contains("window"));
    EXPECT_EQ(j.find("window")->find("width")->asInt(), 1280);
    EXPECT_EQ(j.find("window")->find("height")->asInt(), 720);
    EXPECT_EQ(j.find("vsync")->asBool(), true);
    EXPECT_DOUBLE_EQ(j.find("pi")->asDouble(), 3.14);
    EXPECT_FALSE(j.contains("nope"));
    EXPECT_EQ(j.find("nope"), nullptr);
}

TEST(JsonParse, Arrays) {
    const Json j = parse("[1, 2, [3, 4], \"x\", null, true]");
    ASSERT_TRUE(j.isArray());
    ASSERT_EQ(j.size(), 6u);
    EXPECT_EQ(j.at(0).asInt(), 1);
    ASSERT_TRUE(j.at(2).isArray());
    EXPECT_EQ(j.at(2).at(1).asInt(), 4);
    EXPECT_EQ(j.at(3).asString(), "x");
    EXPECT_TRUE(j.at(4).isNull());
    EXPECT_TRUE(j.at(5).asBool());
}

TEST(JsonParse, StringEscapes) {
    EXPECT_EQ(parse("\"a\\nb\"").asString(), "a\nb");
    EXPECT_EQ(parse("\"a\\\"b\"").asString(), "a\"b");
    EXPECT_EQ(parse("\"a\\\\b\"").asString(), "a\\b");
    EXPECT_EQ(parse("\"\\u0041\\u00e9\"").asString(), std::string("A") + "\xc3\xa9");
    // Surrogate pair for U+1F600 (grinning face).
    const std::string emoji = "\xF0\x9F\x98\x80";
    EXPECT_EQ(parse("\"\\ud83d\\ude00\"").asString(), emoji);
}

TEST(JsonParse, EmptyContainers) {
    EXPECT_TRUE(parse("{}").isObject());
    EXPECT_EQ(parse("{}").memberCount(), 0u);
    EXPECT_TRUE(parse("[]").isArray());
    EXPECT_EQ(parse("[]").size(), 0u);
}

TEST(JsonParse, FailureCases) {
    expectParseFail("");
    expectParseFail("   ");
    expectParseFail("{");
    expectParseFail("}");
    expectParseFail("[1,]");
    expectParseFail("{1: 2}");
    expectParseFail("{\"a\": }");
    expectParseFail("{\"a\" 1}");
    expectParseFail("tru");
    expectParseFail("nul");
    expectParseFail("-");
    expectParseFail("1.2.3");
    expectParseFail("1e");
    expectParseFail("\"unterminated");
    expectParseFail("\"bad \\q escape\"");
    expectParseFail("42 43");
    expectParseFail("[1][2]");
    expectParseFail("{\"a\" \\u0041\"}");
}

TEST(JsonParse, ErrorReportsPosition) {
    const auto r = Json::parse("{\n  \"a\": 1,\n  \"b\": oops\n}");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("line 3"), std::string::npos);
}

TEST(JsonDump, RoundTrip) {
    const std::string original = R"({
        "seed": 123456789,
        "name": "hello \n world \"quoted\"",
        "values": [1, 2.5, -3, true, false, null],
        "nested": { "a": { "b": [] } }
    })";
    const Json j = parse(original);
    const std::string dump = j.dump(-1);
    const Json again = parse(dump);
    EXPECT_EQ(again.dump(-1), dump);
    // Spot-check values survived.
    EXPECT_EQ(again.find("seed")->asInt(), 123456789);
    EXPECT_EQ(again.find("name")->asString(), "hello \n world \"quoted\"");
    EXPECT_EQ(again.find("values")->at(1).asDouble(), 2.5);
    EXPECT_EQ(again.find("nested")->find("a")->find("b")->size(), 0u);
}

TEST(JsonDump, IntegerVsFloatFormatting) {
    EXPECT_EQ(Json(42).dump(), "42");
    EXPECT_EQ(Json(-7).dump(), "-7");
    EXPECT_EQ(Json(2.5).dump(), "2.5");
    // 0.7 must not become "0.69999999999999996" (shortest round-trip form).
    EXPECT_EQ(Json(0.7).dump(), "0.7");
    // Values must survive a dump/parse round trip exactly.
    for (double v : {0.1, 0.2, 1e-7, -3.14159265358979, 123456.789}) {
        const std::string s = Json(v).dump();
        EXPECT_DOUBLE_EQ(Json::parse(s).value().asDouble(), v) << s;
    }
}

TEST(JsonDump, PrettyPrint) {
    const std::string pretty = Json::parse(R"({"a":1})")->dump(2);
    EXPECT_NE(pretty.find("\n"), std::string::npos);
    EXPECT_EQ(Json::parse(pretty)->find("a")->asInt(), 1);
}

TEST(JsonAccess, TypeMismatchReturnsDefault) {
    const Json j = parse("5");
    EXPECT_EQ(j.asString("fallback"), "fallback");
    EXPECT_EQ(j.asBool(true), true);
    EXPECT_FALSE(j.contains("x"));

    const Json s = parse("\"a number\"");
    EXPECT_EQ(s.asInt(-1), -1);
    EXPECT_DOUBLE_EQ(s.asDouble(2.5), 2.5);
}
