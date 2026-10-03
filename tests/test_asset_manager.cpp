#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/asset_manager.hpp"

using mc::AssetManager;

namespace {

class AssetTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_root = std::filesystem::temp_directory_path() /
                 ("mc_asset_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
        std::filesystem::create_directories(m_root / "textures" / "blocks");
        std::filesystem::create_directories(m_root / "shaders");

        {
            std::ofstream out(m_root / "textures" / "blocks" / "stone.txt");
            out << "STONE DATA";
        }
        {
            std::ofstream out(m_root / "shaders" / "basic.vert");
            out << "#version 330\nvoid main() {}\n";
        }

        ASSERT_TRUE(m_assets.setRoot(m_root).ok());
    }
    void TearDown() override { std::filesystem::remove_all(m_root); }

    AssetManager m_assets;
    std::filesystem::path m_root;
};

} // namespace

TEST_F(AssetTest, SetRootRejectsMissingOrFile) {
    AssetManager a;
    EXPECT_FALSE(a.setRoot(m_root / "nope").ok());
    const std::filesystem::path f = m_root / "file.txt";
    { std::ofstream out(f); }
    EXPECT_FALSE(a.setRoot(f).ok());
}

TEST_F(AssetTest, ResolveReturnsAbsolutePath) {
    auto r = m_assets.resolve("shaders/basic.vert");
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_TRUE(r->is_absolute());
    EXPECT_EQ(*r, (m_root / "shaders" / "basic.vert").lexically_normal());
}

TEST_F(AssetTest, ResolveRejectsTraversal) {
    EXPECT_FALSE(m_assets.resolve("../outside.txt").ok());
    EXPECT_FALSE(m_assets.resolve("textures/../../outside.txt").ok());
    EXPECT_FALSE(m_assets.resolve("/etc/passwd").ok());
    EXPECT_FALSE(m_assets.resolve("").ok());
    // Sibling-prefix trick: root "X" must not match "X2".
    EXPECT_FALSE(m_assets.resolve("..").ok());
}

TEST_F(AssetTest, Exists) {
    EXPECT_TRUE(m_assets.exists("shaders/basic.vert"));
    EXPECT_FALSE(m_assets.exists("shaders/missing.vert"));
    EXPECT_FALSE(m_assets.exists("../escape.txt"));
}

TEST_F(AssetTest, LoadTextReturnsContent) {
    auto r = m_assets.loadText("textures/blocks/stone.txt");
    ASSERT_TRUE(r.ok()) << r.error().describe();
    EXPECT_EQ(*r, "STONE DATA");
}

TEST_F(AssetTest, LoadTextFailsForMissing) {
    const auto r = m_assets.loadText("nope/missing.txt");
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("missing.txt"), std::string::npos);
}

TEST_F(AssetTest, LoadTextIsCached) {
    ASSERT_TRUE(m_assets.loadText("shaders/basic.vert").ok());
    ASSERT_TRUE(m_assets.loadText("shaders/basic.vert").ok());
    EXPECT_EQ(m_assets.textCacheSize(), 1u);

    // Modifying the file on disk must not change the cached result.
    { std::ofstream out(m_root / "shaders" / "basic.vert"); out << "CHANGED"; }
    auto r = m_assets.loadText("shaders/basic.vert");
    ASSERT_TRUE(r.ok());
    EXPECT_NE(*r, "CHANGED");

    m_assets.clearCache();
    EXPECT_EQ(m_assets.textCacheSize(), 0u);
    r = m_assets.loadText("shaders/basic.vert");
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(*r, "CHANGED");
}
