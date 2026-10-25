// SAVE-1: the auto-save — the strict config read (save.auto_save_seconds:
// absent = the 120.0 code default, 0 = off, negative / non-finite /
// non-number named and rejected), the real-dt timer step (pure — the
// due / zero-on-success / retry-on-failure semantics), and the
// crash-resilient atomic write (same-directory temp + rename: the
// destination holds either the old COMPLETE file or the new one, a failed
// write never destroys a previously complete save).
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/config.hpp"
#include "core/save.hpp"

using mc::Config;

namespace {

std::filesystem::path scratchDir() {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("mc_save1_test_" + std::to_string(
                          ::testing::UnitTest::GetInstance()->random_seed()));
    std::filesystem::create_directories(dir);
    return dir;
}

// A Config built from written JSON (the test_config.cpp pattern — the
// root data is private; load() is the public construction path, and the
// parser's number handling is what the strict read must validate). The
// stream is CLOSED (flushed) before the load — an open buffered stream
// reads as an empty file.
Config loadConfigJson(const std::filesystem::path& dir,
                      const std::string& json, const std::string& name) {
    const std::filesystem::path p = dir / name;
    {
        std::ofstream out(p);
        out << json;
    }
    auto r = Config::load(p);
    if (!r.ok()) {
        // Report and return the empty config: the assertions below still
        // run against a defined value (the test is already failed).
        ADD_FAILURE() << r.error().describe();
        return Config {};
    }
    return *r;
}

std::string readFile(const std::filesystem::path& p) {
    std::ifstream in(p);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string errOf(const mc::Result<double>& r) {
    return r.error().message;
}

} // namespace

class AutoSaveTest : public ::testing::Test {
protected:
    void SetUp() override { m_dir = scratchDir(); }
    void TearDown() override { std::filesystem::remove_all(m_dir); }
    std::filesystem::path m_dir;
};

// ---------------------------------------------------------------------------
// autoSaveIntervalSeconds: the strict config read
// ---------------------------------------------------------------------------

TEST_F(AutoSaveTest, IntervalDefaultWhenKeyAbsent) {
    auto cfg = loadConfigJson(m_dir, R"({})", "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, mc::kAutoSaveDefaultSeconds);
    EXPECT_DOUBLE_EQ(*r, 120.0);
}

TEST_F(AutoSaveTest, IntervalDefaultWhenSaveSectionHasNoKey) {
    auto cfg = loadConfigJson(m_dir, R"({"save": {"something_else": 1}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 120.0);
}

TEST_F(AutoSaveTest, IntervalZeroIsOff) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": 0}})", "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 0.0);
}

TEST_F(AutoSaveTest, IntervalIntegerValue) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": 3}})", "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 3.0);
}

TEST_F(AutoSaveTest, IntervalFractionalValue) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": 3.5}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_DOUBLE_EQ(*r, 3.5);
}

TEST_F(AutoSaveTest, IntervalNegativeNamed) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": -5}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(errOf(r).find("save.auto_save_seconds"), std::string::npos);
    EXPECT_NE(errOf(r).find("must be >= 0"), std::string::npos);
    EXPECT_NE(errOf(r).find("-5"), std::string::npos);
}

TEST_F(AutoSaveTest, IntervalStringNamed) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": "fast"}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(errOf(r).find("save.auto_save_seconds"), std::string::npos);
    EXPECT_NE(errOf(r).find("must be a number"), std::string::npos);
}

TEST_F(AutoSaveTest, IntervalBoolNamed) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": true}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(errOf(r).find("must be a number"), std::string::npos);
}

TEST_F(AutoSaveTest, IntervalNullNamed) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": null}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(errOf(r).find("must be a number"), std::string::npos);
}

TEST_F(AutoSaveTest, IntervalObjectNamed) {
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": {"v": 1}}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(errOf(r).find("must be a number"), std::string::npos);
}

TEST_F(AutoSaveTest, IntervalNonFiniteNamed) {
    // The parser's strtod turns an overflow literal into inf: a present
    // number that is not finite is named and rejected (the player.cpp
    // reader's isfinite discipline).
    auto cfg = loadConfigJson(m_dir,
                              R"({"save": {"auto_save_seconds": 1e999}})",
                              "cfg.json");
    auto r = mc::autoSaveIntervalSeconds(cfg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(errOf(r).find("must be finite"), std::string::npos);
}

// ---------------------------------------------------------------------------
// autoSaveStep: the real-dt timer semantics (pure)
// ---------------------------------------------------------------------------

TEST_F(AutoSaveTest, StepBelowIntervalNotDue) {
    double accum = 0.0;
    EXPECT_FALSE(mc::autoSaveStep(accum, 1.0, 3.0));
    EXPECT_DOUBLE_EQ(accum, 1.0);
    EXPECT_FALSE(mc::autoSaveStep(accum, 1.0, 3.0));
    EXPECT_DOUBLE_EQ(accum, 2.0);
}

TEST_F(AutoSaveTest, StepAtIntervalDue) {
    double accum = 0.0;
    EXPECT_FALSE(mc::autoSaveStep(accum, 1.0, 3.0));
    EXPECT_FALSE(mc::autoSaveStep(accum, 1.0, 3.0));
    EXPECT_TRUE(mc::autoSaveStep(accum, 1.0, 3.0));
    EXPECT_DOUBLE_EQ(accum, 3.0);
}

TEST_F(AutoSaveTest, StepPartialFramesAccumulate) {
    double accum = 0.0;
    EXPECT_FALSE(mc::autoSaveStep(accum, 2.5, 3.0));
    EXPECT_TRUE(mc::autoSaveStep(accum, 0.5, 3.0));
    EXPECT_DOUBLE_EQ(accum, 3.0);
}

TEST_F(AutoSaveTest, StepOffNeverDue) {
    double accum = 0.0;
    EXPECT_FALSE(mc::autoSaveStep(accum, 100.0, 0.0));
    EXPECT_DOUBLE_EQ(accum, 0.0); // untouched while off
    EXPECT_FALSE(mc::autoSaveStep(accum, 100.0, -1.0));
    EXPECT_DOUBLE_EQ(accum, 0.0);
}

TEST_F(AutoSaveTest, StepFailedSaveRetriesNextFrame) {
    // A DUE save that FAILS leaves the accumulator armed (>= the
    // interval): the next frame reports due again — even at dt 0 — so a
    // broken save path cannot drop the save window.
    double accum = 0.0;
    EXPECT_TRUE(mc::autoSaveStep(accum, 3.0, 3.0));
    // the failure: no reset
    EXPECT_TRUE(mc::autoSaveStep(accum, 0.0, 3.0));
    EXPECT_TRUE(mc::autoSaveStep(accum, 0.25, 3.0));
    // the success: the reset re-arms the full interval
    accum = 0.0;
    EXPECT_FALSE(mc::autoSaveStep(accum, 2.9, 3.0));
}

// ---------------------------------------------------------------------------
// writeAtomicFile: the crash-resilient write (temp + same-dir rename)
// ---------------------------------------------------------------------------

TEST_F(AutoSaveTest, WriteSuccessContentAndNoTmp) {
    const std::filesystem::path slot = m_dir / "slot.json";
    auto r = mc::writeAtomicFile(slot, R"({"format": 19, "seed": 1})" "\n");
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(readFile(slot), R"({"format": 19, "seed": 1})" "\n");
    EXPECT_FALSE(std::filesystem::exists(slot.string() + ".tmp"));
}

TEST_F(AutoSaveTest, WriteCreatesParentDirectories) {
    const std::filesystem::path slot = m_dir / "sub" / "deeper" / "slot.json";
    auto r = mc::writeAtomicFile(slot, "content");
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(readFile(slot), "content");
    EXPECT_FALSE(std::filesystem::exists(slot.string() + ".tmp"));
}

TEST_F(AutoSaveTest, WriteOverwriteReplacesCompleteFile) {
    const std::filesystem::path slot = m_dir / "slot.json";
    {
        std::ofstream pre(slot);
        pre << "old complete save";
    }
    auto r = mc::writeAtomicFile(slot, "new complete save");
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(readFile(slot), "new complete save");
    EXPECT_FALSE(std::filesystem::exists(slot.string() + ".tmp"));
}

TEST_F(AutoSaveTest, WriteReusesStaleTmpFromCrash) {
    // A kill mid-write leaves at most the stale temp: the next write
    // overwrites it and renames, leaving no .tmp behind.
    const std::filesystem::path slot = m_dir / "slot.json";
    const std::filesystem::path tmp = slot.string() + ".tmp";
    {
        std::ofstream t(tmp);
        t << "half-written garbage";
    }
    auto r = mc::writeAtomicFile(slot, "complete save");
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(readFile(slot), "complete save");
    EXPECT_FALSE(std::filesystem::exists(tmp));
}

TEST_F(AutoSaveTest, WriteFailOpenLeavesPreviousSave) {
    // The temp path is a DIRECTORY (the crash-artifact corner): the open
    // fails, the destination file is untouched, the error names the
    // DESTINATION (the caller's concept, not the temp).
    const std::filesystem::path slot = m_dir / "slot.json";
    {
        std::ofstream pre(slot); // closed before the write: the previous
        pre << "previous complete save"; // save must be complete + flushed
    }
    std::filesystem::create_directories(slot.string() + ".tmp");
    auto r = mc::writeAtomicFile(slot, "new save");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("slot.json"), std::string::npos);
    EXPECT_EQ(readFile(slot), "previous complete save");
}

TEST_F(AutoSaveTest, WriteFailRenameLeavesDestinationUntouched) {
    // The destination is an existing (empty) DIRECTORY: temp + close
    // succeed, the rename fails (file onto directory), the temp is
    // removed and the destination survives as-is.
    const std::filesystem::path dest = m_dir / "dest";
    std::filesystem::create_directories(dest);
    auto r = mc::writeAtomicFile(dest, "content");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("cannot move"), std::string::npos);
    EXPECT_NE(r.error().message.find("dest"), std::string::npos);
    EXPECT_TRUE(std::filesystem::is_directory(dest));
    EXPECT_FALSE(
        std::filesystem::exists(dest.string() + ".tmp")); // cleaned up
    // the destination directory is still empty (untouched)
    std::filesystem::recursive_directory_iterator it(dest);
    EXPECT_TRUE(it == std::filesystem::recursive_directory_iterator {});
}

TEST_F(AutoSaveTest, WriteFailIntoFilelessLocationNamesPath) {
    // The parent path component is a FILE: create_directories fails, the
    // temp open fails, the error names the destination.
    const std::filesystem::path blocker = m_dir / "blocker";
    {
        std::ofstream b(blocker);
        b << "x";
    }
    const std::filesystem::path slot = blocker / "slot.json";
    auto r = mc::writeAtomicFile(slot, "content");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("slot.json"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(slot.string() + ".tmp"));
    EXPECT_EQ(readFile(blocker), "x"); // the blocker is untouched
}
