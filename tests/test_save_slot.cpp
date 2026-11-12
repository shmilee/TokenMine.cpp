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

#include "core/save.hpp"
#include "core/save_slot.hpp"

using mc::copySaveSlot;
using mc::deleteSaveSlot;
using mc::isValidSlotName;
using mc::listSaveSlots;
using mc::readSaveSlotSummary;
using mc::renameSaveSlot;
using mc::slotReadPath;
using mc::slotWritePath;

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
    // V3-18: the two-stage find names a missing slot directly (both
    // extensions absent) — the pre-V3-18 "cannot open" naming's twin,
    // the probe's earlier fail.
    EXPECT_NE(r->problem.find("no such save slot"), std::string::npos)
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

// V3-7: the gzip slot (this build's write default) — the summary reads
// through the same magic probe the game load uses (the zero-migration
// twin: a legacy plaintext slot reads straight through, unchanged).
TEST(SaveSlotTest, GzipSlotSummaryIsReadable) {
    const auto dir = scratchDir();
    const std::string plaintext = saveJson(29, 123456789, 0.25, 22.5, 38.0,
                                           0.5, 9.0);
    auto gz = mc::gzipEncode(plaintext);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    writeFile(dir / "g.json", *gz);
    auto a = readSaveSlotSummary(dir, "g");
    ASSERT_TRUE(a.ok());
    EXPECT_TRUE(a->readable);
    EXPECT_EQ(a->problem, "");
    EXPECT_EQ(a->name, "g");
    EXPECT_EQ(a->format, 29);
    EXPECT_EQ(a->seed, 123456789);
    EXPECT_DOUBLE_EQ(a->time, 0.25);
    EXPECT_DOUBLE_EQ(a->posX, 22.5);
    EXPECT_DOUBLE_EQ(a->posY, 38.0);
    EXPECT_DOUBLE_EQ(a->posZ, 0.5);
    EXPECT_DOUBLE_EQ(a->health, 9.0);
    // The plaintext twin of the SAME content reads identically — the
    // summary is a pure function of the decoded contents.
    writeFile(dir / "p.json", plaintext);
    auto b = readSaveSlotSummary(dir, "p");
    ASSERT_TRUE(b.ok());
    EXPECT_TRUE(b->readable);
    EXPECT_EQ(a->format, b->format);
    EXPECT_EQ(a->seed, b->seed);
}

// V3-7: a CORRUPT gzip slot surfaces with the named problem (the file
// stays in the list, never hidden — the existing problem-naming
// discipline), and a cut-mid-body container decodes to the corrupt-
// body naming, not a crash.
TEST(SaveSlotTest, CorruptGzipSlotSurfacesTheProblem) {
    const auto dir = scratchDir();
    const std::string plaintext = saveJson(29, 7, 0.5, 0.0, 64.0, 0.0, 20.0);
    auto gz = mc::gzipEncode(plaintext);
    ASSERT_TRUE(gz.ok()) << gz.error().message;
    // Truncated to under the 20-byte minimum container.
    writeFile(dir / "t.json", gz->substr(0, 15));
    auto t = readSaveSlotSummary(dir, "t");
    ASSERT_TRUE(t.ok());
    EXPECT_FALSE(t->readable);
    EXPECT_NE(t->problem.find("invalid gzip:"), std::string::npos)
        << t->problem;
    EXPECT_NE(t->problem.find("truncated gzip"), std::string::npos)
        << t->problem;
    // Cut mid-body (the trailer is gone with the rest of the stream).
    writeFile(dir / "c.json", gz->substr(0, gz->size() / 2));
    auto c = readSaveSlotSummary(dir, "c");
    ASSERT_TRUE(c.ok());
    EXPECT_FALSE(c->readable);
    EXPECT_NE(c->problem.find("invalid gzip:"), std::string::npos)
        << c->problem;
    EXPECT_NE(c->problem.find("corrupt gzip body"), std::string::npos)
        << c->problem;
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
    // The copy is byte-faithful (V3-18: the destination is written in
    // the NEW form — the legacy source's bytes, the .json.gz's name).
    auto ok = copySaveSlot(dir, "a", "b");
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_EQ(readFile(dir / "b.json.gz"), readFile(dir / "a.json"));
    // An existing destination is a naming (the source is untouched).
    auto exist = copySaveSlot(dir, "a", "b");
    EXPECT_FALSE(exist.ok());
    EXPECT_NE(exist.error().message.find("already exists"),
              std::string::npos);
    EXPECT_EQ(readFile(dir / "b.json.gz"), readFile(dir / "a.json"));
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
    // V3-18: the rename moves the slot into the NEW form (.json.gz).
    EXPECT_FALSE(std::filesystem::exists(dir / "b.json"));
    EXPECT_EQ(readFile(dir / "b.json.gz"), bytes);
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

// ---------------------------------------------------------------------------
// V3-18: the extension's rename (the user's 11-08 report — the .json's
// name showed nothing of the gzip content). The write's NEW form is
// .json.gz; the READ is a two-stage find (.json.gz first, the legacy
// .json after = zero migration — a legacy slot loads, the next write
// re-names it).
// ---------------------------------------------------------------------------

TEST(SaveSlotTest, V318_WritePathIsTheGzExtension) {
    const auto dir = scratchDir();
    EXPECT_EQ(slotWritePath(dir, "abc"), dir / "abc.json.gz");
    // The legacy name is never a write target (a stale .json is only
    // ever READ, then replaced by the next write's new form).
    EXPECT_NE(slotWritePath(dir, "abc").string().find(".json.gz"),
              std::string::npos);
}

TEST(SaveSlotTest, V318_ReadPathTwoStageFind) {
    const auto dir = scratchDir();
    // Neither = the empty path (the caller's naming).
    EXPECT_TRUE(slotReadPath(dir, "ghost").empty());
    // The legacy .json alone is found (zero migration's read leg).
    writeFile(dir / "legacy.json", "x");
    EXPECT_EQ(slotReadPath(dir, "legacy"), dir / "legacy.json");
    // The current .json.gz alone is found.
    writeFile(dir / "modern.json.gz", "x");
    EXPECT_EQ(slotReadPath(dir, "modern"), dir / "modern.json.gz");
    // BOTH forms = the .json.gz wins (the current form's summary).
    writeFile(dir / "both.json.gz", "gz-bytes");
    writeFile(dir / "both.json", "legacy-bytes");
    EXPECT_EQ(slotReadPath(dir, "both"), dir / "both.json.gz");
    EXPECT_EQ(readFile(slotReadPath(dir, "both")), "gz-bytes");
}

TEST(SaveSlotTest, V318_ListEnumeratesBothExtensionsDeduped) {
    const auto dir = scratchDir();
    writeFile(dir / "a.json.gz", "{}");
    writeFile(dir / "b.json", "{}");
    writeFile(dir / "c.json.gz", "{}");
    writeFile(dir / "c.json", "{}"); // a slot holding BOTH
    writeFile(dir / "d.txt", "{}"); // not a slot
    std::filesystem::create_directories(dir / "e.json"); // a DIR
    auto names = listSaveSlots(dir);
    ASSERT_TRUE(names.ok()) << names.error().message;
    // a (new form), b (legacy), c (both = ONCE) — sorted, deduped.
    EXPECT_EQ(*names, (std::vector<std::string> {"a", "b", "c"}));
    // The both-form slot's summary comes from the .json.gz (the current
    // form's win in the two-stage find).
    writeFile(dir / "c.json.gz",
              saveJson(22, 111, 0.1, 1, 1, 1, 20.0));
    writeFile(dir / "c.json", saveJson(99, 222, 0.9, 9, 9, 9, 1.0));
    auto sum = readSaveSlotSummary(dir, "c");
    ASSERT_TRUE(sum.ok());
    EXPECT_EQ(sum->format, 22);
    EXPECT_EQ(sum->seed, 111);
}

TEST(SaveSlotTest, V318_CopyAndRenameMigrateToTheNewForm) {
    const auto dir = scratchDir();
    writeFile(dir / "src.json", saveJson(22, 7, 0.5, 1, 2, 3, 20.0));
    // Copy: a legacy source reads, the destination is the NEW form.
    auto cp = copySaveSlot(dir, "src", "dst");
    ASSERT_TRUE(cp.ok()) << cp.error().message;
    EXPECT_FALSE(std::filesystem::exists(dir / "dst.json"));
    EXPECT_EQ(readFile(dir / "dst.json.gz"), readFile(dir / "src.json"));
    // Rename: the slot moves to the NEW form, the legacy twin is gone.
    auto rn = renameSaveSlot(dir, "src", "ren");
    ASSERT_TRUE(rn.ok()) << rn.error().message;
    EXPECT_FALSE(std::filesystem::exists(dir / "src.json"));
    EXPECT_EQ(readFile(dir / "ren.json.gz"),
              saveJson(22, 7, 0.5, 1, 2, 3, 20.0));
    // A rename of a BOTH-form slot consumes the whole slot (the twin's
    // deletion — no lingering file under the old name).
    writeFile(dir / "tw.json.gz", "gz");
    writeFile(dir / "tw.json", "legacy");
    auto rn2 = renameSaveSlot(dir, "tw", "tw2");
    ASSERT_TRUE(rn2.ok()) << rn2.error().message;
    EXPECT_FALSE(std::filesystem::exists(dir / "tw.json"));
    EXPECT_FALSE(std::filesystem::exists(dir / "tw.json.gz"));
    EXPECT_EQ(readFile(dir / "tw2.json.gz"), "gz"); // the .json.gz's won
}

TEST(SaveSlotTest, V318_DeleteConsumesBothForms) {
    const auto dir = scratchDir();
    writeFile(dir / "a.json.gz", "gz");
    writeFile(dir / "a.json", "legacy");
    auto ok = deleteSaveSlot(dir, "a");
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_FALSE(std::filesystem::exists(dir / "a.json"));
    EXPECT_FALSE(std::filesystem::exists(dir / "a.json.gz"));
    // A double delete is a naming (the slot is gone).
    auto again = deleteSaveSlot(dir, "a");
    EXPECT_FALSE(again.ok());
    EXPECT_NE(again.error().message.find("no such save slot"),
              std::string::npos);
}
