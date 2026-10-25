// CRAFT-2: item-model mesh geometry tests.
//
// makeItemModelGeometry is a pure function of the parsed model data:
// box elements -> six face quads (the kCubeFaces corner convention),
// centered in the unit-cube model space, full sky light. The winding
// invariant mirrors the item-cube test (hard rule 9: the face table is
// regression-guarded), and the last test pins the content authoring
// convention (no face-coincident boxes — they z-fight) on the SHIPPED
// 16 models.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "entity/item_model_mesh.hpp"
#include "item/item.hpp"
#include "item/item_registry.hpp"

using namespace mc;

namespace {

Item::ModelElement element(float x0, float y0, float z0, float x1, float y1,
                           float z1, bool withUv = false,
                           float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f,
                           float v1 = 1.0f) {
    Item::ModelElement el;
    el.from = {x0, y0, z0};
    el.to = {x1, y1, z1};
    if (withUv) {
        el.uv0 = {u0, v0};
        el.uv1 = {u1, v1};
    }
    return el;
}

Item::ItemModel oneElementModel(Item::ModelElement el) {
    Item::ItemModel model;
    model.texture = "t";
    model.elements = {std::move(el)};
    return model;
}

bool uvInRect(const Vec2& uv, float u0, float v0, float u1, float v1,
              float eps = 1e-6f) {
    return uv.x >= u0 - eps && uv.x <= u1 + eps && uv.y >= v0 - eps &&
           uv.y <= v1 + eps;
}

std::string readAsset(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

} // namespace

TEST(ItemModelMeshTest, FullUnitBoxIsCenteredAndSized) {
    const ItemMeshGeometry g =
        makeItemModelGeometry(oneElementModel(element(0.0f, 0.0f, 0.0f,
                                                      1.0f, 1.0f, 1.0f)));
    // 6 faces x 4 vertices / 6 indices per element.
    EXPECT_EQ(g.vertices.size(), 24u);
    EXPECT_EQ(g.indices.size(), 36u);
    // Centered in the unit-cube model space: every coordinate in [-0.5,
    // 0.5], and every face plane at +/-0.5 on every axis is present.
    bool planes[3][2] = {};
    for (const MeshVertex& v : g.vertices) {
        EXPECT_LE(std::abs(v.position.x), 0.5f + 1e-6f);
        EXPECT_LE(std::abs(v.position.y), 0.5f + 1e-6f);
        EXPECT_LE(std::abs(v.position.z), 0.5f + 1e-6f);
        // Full sky light baked in (the block-cube-drop convention: the
        // shader's day/night scale then shades it like the world).
        EXPECT_FLOAT_EQ(v.skyLight, 1.0f);
        EXPECT_FLOAT_EQ(v.blockLight, 0.0f);
        if (v.position.x > 0.5f - 1e-6f) {
            planes[0][1] = true;
        } else if (v.position.x < -0.5f + 1e-6f) {
            planes[0][0] = true;
        }
        if (v.position.y > 0.5f - 1e-6f) {
            planes[1][1] = true;
        } else if (v.position.y < -0.5f + 1e-6f) {
            planes[1][0] = true;
        }
        if (v.position.z > 0.5f - 1e-6f) {
            planes[2][1] = true;
        } else if (v.position.z < -0.5f + 1e-6f) {
            planes[2][0] = true;
        }
    }
    for (int a = 0; a < 3; ++a) {
        EXPECT_TRUE(planes[a][0]) << "axis " << a << " min plane missing";
        EXPECT_TRUE(planes[a][1]) << "axis " << a << " max plane missing";
    }
}

TEST(ItemModelMeshTest, WindingIsCounterClockwiseFromOutside) {
    // Same invariant as the item cube (hard rule 9): for every face,
    // cross(corner1 - corner0, corner2 - corner0) must point along the
    // face normal (4 vertices per face, 6 faces).
    const ItemMeshGeometry g = makeItemModelGeometry(
        oneElementModel(element(0.1f, 0.2f, 0.3f, 0.7f, 0.8f, 0.9f)));
    for (std::size_t face = 0; face < 6; ++face) {
        const std::size_t base = face * 4;
        const Vec3 c0 = g.vertices[base].position;
        const Vec3 c1 = g.vertices[base + 1].position;
        const Vec3 c2 = g.vertices[base + 2].position;
        const Vec3 crossDir = cross(c1 - c0, c2 - c0);
        EXPECT_GT(dot(crossDir, g.vertices[base].normal), 0.0f)
            << "face " << face;
        // All four vertices of a face share its normal.
        for (int i = 1; i < 4; ++i) {
            EXPECT_EQ(g.vertices[base + i].normal, g.vertices[base].normal);
        }
    }
}

TEST(ItemModelMeshTest, SubBoxIsOffsetInsideTheUnitCube) {
    // Element (0.25, 0, 0.5)-(0.75, 1, 1): after centering, x spans
    // [-0.25, 0.25], y spans [-0.5, 0.5], z spans [0, 0.5].
    const ItemMeshGeometry g = makeItemModelGeometry(
        oneElementModel(element(0.25f, 0.0f, 0.5f, 0.75f, 1.0f, 1.0f)));
    float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f, mnz = 1e9f,
          mxz = -1e9f;
    for (const MeshVertex& v : g.vertices) {
        mnx = std::min(mnx, v.position.x);
        mxx = std::max(mxx, v.position.x);
        mny = std::min(mny, v.position.y);
        mxy = std::max(mxy, v.position.y);
        mnz = std::min(mnz, v.position.z);
        mxz = std::max(mxz, v.position.z);
    }
    EXPECT_FLOAT_EQ(mnx, -0.25f);
    EXPECT_FLOAT_EQ(mxx, 0.25f);
    EXPECT_FLOAT_EQ(mny, -0.5f);
    EXPECT_FLOAT_EQ(mxy, 0.5f);
    EXPECT_FLOAT_EQ(mnz, 0.0f);
    EXPECT_FLOAT_EQ(mxz, 0.5f);
}

TEST(ItemModelMeshTest, UvCornersMapToTheElementRect) {
    // With an explicit uv rect, every face's four corner UVs are exactly
    // the rect's corners (the item-cube mapping); without one, the full
    // texture. A slot/UV mix-up would land a corner outside the rect.
    const ItemMeshGeometry g = makeItemModelGeometry(
        oneElementModel(element(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, true,
                                0.1f, 0.2f, 0.9f, 0.8f)));
    for (std::size_t face = 0; face < 6; ++face) {
        const std::size_t base = face * 4;
        EXPECT_EQ(g.vertices[base].uv, Vec2(0.1f, 0.8f)) << "face " << face;
        EXPECT_EQ(g.vertices[base + 1].uv, Vec2(0.9f, 0.8f)) << "face "
                                                              << face;
        EXPECT_EQ(g.vertices[base + 2].uv, Vec2(0.9f, 0.2f)) << "face "
                                                             << face;
        EXPECT_EQ(g.vertices[base + 3].uv, Vec2(0.1f, 0.2f)) << "face "
                                                             << face;
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(uvInRect(g.vertices[base + i].uv, 0.1f, 0.2f, 0.9f,
                                 0.8f));
        }
    }
    const ItemMeshGeometry gFull = makeItemModelGeometry(
        oneElementModel(element(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f)));
    for (const MeshVertex& v : gFull.vertices) {
        EXPECT_TRUE(uvInRect(v.uv, 0.0f, 0.0f, 1.0f, 1.0f));
    }
    // The full-texture default is exactly the unit square corners.
    EXPECT_EQ(gFull.vertices[0].uv, Vec2(0.0f, 1.0f));
    EXPECT_EQ(gFull.vertices[1].uv, Vec2(1.0f, 1.0f));
    EXPECT_EQ(gFull.vertices[2].uv, Vec2(1.0f, 0.0f));
    EXPECT_EQ(gFull.vertices[3].uv, Vec2(0.0f, 0.0f));
}

TEST(ItemModelMeshTest, ElementsConcatenateInOrder) {
    Item::ItemModel model;
    model.texture = "t";
    model.elements = {element(0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f),
                      element(0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f)};
    const ItemMeshGeometry g = makeItemModelGeometry(model);
    EXPECT_EQ(g.vertices.size(), 48u);
    EXPECT_EQ(g.indices.size(), 72u);
    // Element 1's box is centered at (-0.25, -0.25, -0.25); element 2's
    // at (+0.25, +0.25, +0.25). The first 24 vertices are element 1, the
    // next 24 element 2.
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for (int i = 0; i < 24; ++i) {
        cx += g.vertices[i].position.x;
        cy += g.vertices[i].position.y;
        cz += g.vertices[i].position.z;
    }
    EXPECT_FLOAT_EQ(cx / 24.0f, -0.25f);
    EXPECT_FLOAT_EQ(cy / 24.0f, -0.25f);
    EXPECT_FLOAT_EQ(cz / 24.0f, -0.25f);
    cx = cy = cz = 0.0f;
    for (std::size_t i = 24; i < 48; ++i) {
        cx += g.vertices[i].position.x;
        cy += g.vertices[i].position.y;
        cz += g.vertices[i].position.z;
    }
    EXPECT_FLOAT_EQ(cx / 24.0f, 0.25f);
    EXPECT_FLOAT_EQ(cy / 24.0f, 0.25f);
    EXPECT_FLOAT_EQ(cz / 24.0f, 0.25f);
    // Indices stay inside their element's vertex range.
    for (std::size_t i = 0; i < 36; ++i) {
        EXPECT_LT(g.indices[i], 24u);
    }
    for (std::size_t i = 36; i < 72; ++i) {
        EXPECT_GE(g.indices[i], 24u);
        EXPECT_LT(g.indices[i], 48u);
    }
}

TEST(ItemModelMeshTest, ShippedModelsHaveNoFaceCoincidentBoxes) {
    // Content authoring convention (the z-fight guard): no two elements
    // of a shipped model may share a face plane on one axis with a
    // positive-area overlap on the other two (coplanar overlapping quads
    // flicker). Overlapping VOLUMES (nested slabs) are fine — the interior
    // faces are strictly inside the other box and depth-occluded.
    const std::filesystem::path dir = "assets/data/items";
    ASSERT_TRUE(std::filesystem::is_directory(dir)) << "run from the repo root";
    int modelCount = 0;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        auto parsed = Json::parse(readAsset(file.generic_string()));
        ASSERT_TRUE(parsed.ok()) << file.generic_string();
        const Json* m = parsed->find("model");
        if (m == nullptr) {
            continue;
        }
        ++modelCount;
        ItemRegistry items;
        auto reg = items.registerItem(*parsed);
        ASSERT_TRUE(reg.ok()) << file.generic_string() << ": "
                              << reg.error().message;
        const Item& it = items.at(*reg);
        ASSERT_TRUE(it.model.has_value());
        const std::vector<Item::ModelElement>& els = it.model->elements;
        for (std::size_t i = 0; i < els.size(); ++i) {
            for (std::size_t j = i + 1; j < els.size(); ++j) {
                const float amin[3] = {els[i].from.x, els[i].from.y,
                                       els[i].from.z};
                const float amax[3] = {els[i].to.x, els[i].to.y,
                                       els[i].to.z};
                const float bmin[3] = {els[j].from.x, els[j].from.y,
                                       els[j].from.z};
                const float bmax[3] = {els[j].to.x, els[j].to.y,
                                       els[j].to.z};
                for (int axis = 0; axis < 3; ++axis) {
                    const bool planeShared =
                        amax[axis] == bmin[axis] ||
                        amin[axis] == bmax[axis];
                    if (!planeShared) {
                        continue;
                    }
                    // Positive-area overlap on the other two axes?
                    bool positiveArea = true;
                    for (int o = 0; o < 3 && positiveArea; ++o) {
                        if (o == axis) {
                            continue;
                        }
                        const float lo = std::max(amin[o], bmin[o]);
                        const float hi = std::min(amax[o], bmax[o]);
                        positiveArea = hi - lo > 1e-6f;
                    }
                    EXPECT_FALSE(positiveArea)
                        << it.name << ": elements " << i << " and " << j
                        << " share a face plane on axis " << axis
                        << " with an overlapping area";
                }
            }
        }
    }
    EXPECT_EQ(modelCount, 16) << "the shipped model set is exactly 16";
}
