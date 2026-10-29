// SAVE-3: save slot management — the strict slot name (the --save
// charset, which also kills path traversal), the deterministic list
// (name-sorted, .json-only, unreadable files SURFACE with the naming
// instead of vanishing), the content-only summary (no mtime — repeated
// reads print identically), and the byte-faithful copy / atomic rename /
// delete (the destination-exists and source-missing namings; a failed
// copy never leaves a half-written destination).
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/save_slot.hpp"

using mc::copySaveSlot;
using mc::deleteSaveSlot;
using mc::isValidSlotName;
using mc::listSaveSlots;
using mc::readSaveSlotSummary;
using mc::renameSaveSlot;

namespace {

std::filesystem::path scratchDir() {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("mc_save3_test_" + std::to_string(
                          ::testing::UnitTest::GetInstance()->random_seed()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void writeFile(const std::filesystem::path& p, const std::string& content) {
    std::ofstream out(p);
    out << content;
}

std::string readFile(const std::filesystem::path& p) {
    std::ifstream in(p);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// A minimal save-shaped JSON (the summary fields the list reads).
std::string saveJson(int format, long long seed, double time,
                     double px, double py, double pz, double hp) {
    std::ostringstream os;
    os << "{\"format\": " << format << ", \"seed\": " << seed
       << ", \"time\": " << time
       << ", \"player\": {\"pos\": [" << px << ", " << py << ", " << pz
       << "], \"health\": " << hp << "}}";
    return os.str();
}

} // namespace

TEST(SaveSlotTest, ValidSlotNamesAreTheSaveCharset) {
    for (const char* ok :
         {"a", "B", "0", "_", "-", "a-b_c9", "g141a", "Zz_0-9"}) {
        EXPECT_TRUE(isValidSlotName(ok)) << ok;
    }
    for (const char* bad :
         {"", "a/b", "a b", "a.b", "../x", "a\tb", "中"}) {
        EXPECT_FALSE(isValidSlotName(bad));
    }
}

TEST(SaveSlotTest, ListIsSortedJsonOnlyAndMissingDirIsEmpty) {
    const auto dir = scratchDir();
    auto empty = listSaveSlots(dir / "no_such_dir");
    ASSERT_TRUE(empty.ok());
    EXPECT_TRUE(empty->empty());
    writeFile(dir / "b.json", "{}");
    writeFile(dir / "a.json", "{}");
    writeFile(dir / "c.json", "{}");
    writeFile(dir / "notes.txt", "not a slot");
    writeFile(dir / "README.md", "not a slot");
    std::filesystem::create_directories(dir / "sub.json"); // a DIR named
    auto r = listSaveSlots(dir); // .json is not a slot file
    ASSERT_TRUE(r.ok());
    ASSERT_EQ(r->size(), 3u);
    EXPECT_EQ((*r)[0], "a");
    EXPECT_EQ((*r)[1], "b");
    EXPECT_EQ((*r)[2], "c");
}

TEST(SaveSlotTest, SummaryReadsTheContentNotTheClock) {
    const auto dir = scratchDir();
    writeFile(dir / "s1.json", saveJson(22, 123456789, 0.25, 22.5, 38.0, 0.5, 9.0));
    auto a = readSaveSlotSummary(dir, "s1");
    ASSERT_TRUE(a.ok());
    EXPECT_TRUE(a->readable);
    EXPECT_EQ(a->problem, "");
    EXPECT_EQ(a->name, "s1");
    EXPECT_EQ(a->format, 22);
    EXPECT_EQ(a->seed, 123456789);
    EXPECT_DOUBLE_EQ(a->time, 0.25);
    EXPECT_DOUBLE_EQ(a->posX, 22.5);
    EXPECT_DOUBLE_EQ(a->posY, 38.0);
    EXPECT_DOUBLE_EQ(a->posZ, 0.5);
    EXPECT_DOUBLE_EQ(a->health, 9.0);
    // A second read (no mtime, no wall clock anywhere) is identical —
    // the list is a pure function of the file contents.
    auto b = readSaveSlotSummary(dir, "s1");
    ASSERT_TRUE(b.ok());
    EXPECT_EQ(*a, *b);
}

TEST(SaveSlotTest, SummaryMissingFileIsANamingNotAError) {
    const auto dir = scratchDir();
    auto r = readSaveSlotSummary(dir, "ghost");
    ASSERT_TRUE(r.ok()); // the list must SURFACE the file state
    EXPECT_FALSE(r->readable);
    EXPECT_NE(r->problem.find("cannot open"), std::string::npos)
        << r->problem;
}

TEST(SaveSlotTest, SummaryUnreadableFileSurfacesTheProblem) {
    const auto dir = scratchDir();
    writeFile(dir / "bad.json", "{ not json");
    auto r = readSaveSlotSummary(dir, "bad");
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(r->readable);
    EXPECT_NE(r->problem.find("invalid JSON"), std::string::npos)
        << r->problem;
    writeFile(dir / "nofmt.json", "{\"seed\": 1}");
    r = readSaveSlotSummary(dir, "nofmt");
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(r->readable);
    EXPECT_NE(r->problem.find("format"), std::string::npos) << r->problem;
}

TEST(SaveSlotTest, SummaryRejectsInvalidNames) {
    const auto dir = scratchDir();
    auto r = readSaveSlotSummary(dir, "../etc");
    EXPECT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("invalid save slot name"),
              std::string::npos);
}

TEST(SaveSlotTest, CopyIsByteFaithfulAndRefusesExistingDest) {
    const auto dir = scratchDir();
    writeFile(dir / "a.json", saveJson(22, 7, 0.5, 1, 2, 3, 20.0));
    // Missing source is a naming.
    auto miss = copySaveSlot(dir, "ghost", "b");
    EXPECT_FALSE(miss.ok());
    EXPECT_NE(miss.error().message.find("no such save slot"),
              std::string::npos);
    // The copy is byte-faithful.
    auto ok = copySaveSlot(dir, "a", "b");
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_EQ(readFile(dir / "b.json"), readFile(dir / "a.json"));
    // An existing destination is a naming (the source is untouched).
    auto exist = copySaveSlot(dir, "a", "b");
    EXPECT_FALSE(exist.ok());
    EXPECT_NE(exist.error().message.find("already exists"),
              std::string::npos);
    EXPECT_EQ(readFile(dir / "b.json"), readFile(dir / "a.json"));
    // Invalid names are namings on both sides.
    auto bad = copySaveSlot(dir, "a/b", "b");
    EXPECT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("invalid save slot name"),
              std::string::npos);
    bad = copySaveSlot(dir, "a", "b c");
    EXPECT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("invalid save slot name"),
              std::string::npos);
}

TEST(SaveSlotTest, CopyNeverLeavesHalfWrittenDestinations) {
    const auto dir = scratchDir();
    writeFile(dir / "a.json", saveJson(22, 7, 0.5, 1, 2, 3, 20.0));
    // A failed copy (existing destination) must not create/modify any
    // .tmp or partial file in the directory.
    writeFile(dir / "b.json", "old");
    auto fail = copySaveSlot(dir, "a", "b");
    EXPECT_FALSE(fail.ok());
    std::vector<std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        files.push_back(e.path().filename().string());
    }
    std::sort(files.begin(), files.end());
    EXPECT_EQ(files, (std::vector<std::string> {"a.json", "b.json"}));
    EXPECT_EQ(readFile(dir / "b.json"), "old");
}

TEST(SaveSlotTest, RenameMovesTheBytesAndRefusesExistingDest) {
    const auto dir = scratchDir();
    writeFile(dir / "a.json", saveJson(22, 7, 0.5, 1, 2, 3, 20.0));
    auto miss = renameSaveSlot(dir, "ghost", "b");
    EXPECT_FALSE(miss.ok());
    EXPECT_NE(miss.error().message.find("no such save slot"),
              std::string::npos);
    auto exist = renameSaveSlot(dir, "a", "a");
    EXPECT_FALSE(exist.ok());
    EXPECT_NE(exist.error().message.find("already exists"),
              std::string::npos);
    const std::string bytes = readFile(dir / "a.json");
    auto ok = renameSaveSlot(dir, "a", "b");
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_FALSE(std::filesystem::exists(dir / "a.json"));
    EXPECT_EQ(readFile(dir / "b.json"), bytes);
}

TEST(SaveSlotTest, DeleteRemovesAndRefusesMissing) {
    const auto dir = scratchDir();
    auto miss = deleteSaveSlot(dir, "ghost");
    EXPECT_FALSE(miss.ok());
    EXPECT_NE(miss.error().message.find("no such save slot"),
              std::string::npos);
    auto bad = deleteSaveSlot(dir, "a/b");
    EXPECT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("invalid save slot name"),
              std::string::npos);
    writeFile(dir / "a.json", "x");
    auto ok = deleteSaveSlot(dir, "a");
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_FALSE(std::filesystem::exists(dir / "a.json"));
}
