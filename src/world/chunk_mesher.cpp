#include "world/chunk_mesher.hpp"

#include <array>

#include "block/cube_face_table.hpp"
#include "core/math/vec3.hpp"
#include "world/growth.hpp"
#include "world/lighting.hpp"

namespace mc {

namespace {

// Mask-coordinate assignment per face direction (kFaceSlices is indexed
// like kCubeFaces: +Y, -Y, +Z, -Z, +X, -X). The slice axis is the face
// normal's axis; (u, v) are the two in-plane cell axes of the 16x16 mask
// grid. The relationship between the mask directions and the face's
// corner-table edge directions (+u vs the c0->c1 edge, +v vs the c0->c3
// edge) is derived from the table at emission time, so any consistent
// (u, v) choice stays correct.
struct FaceSlice {
    int axis;  // 0=x, 1=y, 2=z — the face normal's axis
    int sign;  // +1 (normal along +axis) or -1
    int uAxis; // mask u runs along this cell axis
    int vAxis; // mask v runs along this cell axis
};

constexpr FaceSlice kFaceSlices[6] = {
    {1, +1, 2, 0},  // +Y: u = z, v = x
    {1, -1, 2, 0},  // -Y: u = z, v = x
    {2, +1, 0, 1},  // +Z: u = x, v = y
    {2, -1, 0, 1},  // -Z: u = x, v = y
    {0, +1, 2, 1},  // +X: u = z, v = y
    {0, -1, 2, 1},  // -X: u = z, v = y
};

constexpr Vec3 kAxisUnit[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                               {0.0f, 0.0f, 1.0f}};

} // namespace

ChunkMesher::ChunkMesher(const World& world, const BlockRegistry& blocks,
                         const AtlasLayout& layout)
    : m_world(world), m_blocks(blocks), m_layout(layout) {}

ChunkMeshData ChunkMesher::mesh(const ChunkPos& pos) const {
    ChunkMeshData out;
    const Chunk* chunk = m_world.findChunk(pos);
    if (chunk == nullptr) {
        return out;
    }

    const BlockPos origin = chunkOrigin(pos);

    // Fluids: per-cell level-scaled cubes (not merged — see the header).
    // Fluid faces are textured from the registry's reference block of the
    // cell's KIND (properties.fluid names the kind); a kind without one
    // renders nothing.
    const std::optional<BlockID> waterId =
        m_blocks.fluidBlock(fluidKindName(FluidKind::Water));
    const std::optional<BlockID> lavaId =
        m_blocks.fluidBlock(fluidKindName(FluidKind::Lava));
    const std::optional<BlockID> honeyId =
        m_blocks.fluidBlock(fluidKindName(FluidKind::Honey));
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                if (chunk->get(lx, ly, lz) != BlockID::air()) {
                    continue;
                }
                const std::uint8_t code = chunk->fluidAt(lx, ly, lz);
                if (code == 0) {
                    continue;
                }
                const FluidKind kind = fluidKindOf(code);
                const std::optional<BlockID> fluidId =
                    (kind == FluidKind::Water) ? waterId
                    : (kind == FluidKind::Lava) ? lavaId : honeyId;
                if (fluidId == std::nullopt) {
                    continue;
                }
                emitFluid(out, m_blocks.at(*fluidId), origin, lx, ly, lz,
                          fluidLevelOf(code), kind);
            }
        }
    }

    // Cross plants (Phase 45): two crossing quads per cell, fixed cell
    // order (the vertex order is deterministic across runs). No neighbor
    // culling — the quads are strictly inside the cell, and the depth test
    // against already-drawn opaque geometry hides buried ones.
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const BlockID id = chunk->get(lx, ly, lz);
                if (id == BlockID::air()) {
                    continue;
                }
                const Block& block = m_blocks.at(id);
                if (block.model.shape == BlockModel::Shape::Cross) {
                    emitCross(out, block, origin, lx, ly, lz);
                }
            }
        }
    }

    // Slabs (Phase 85): bottom half-blocks, one pass in fixed cell order
    // (the vertex order is deterministic across runs). A slab's geometry is
    // its own (emitSlab scales the unit-cube faces to y 0..0.5) — it is NOT
    // a full cube, so the greedy block-face pass below skips it.
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const BlockID id = chunk->get(lx, ly, lz);
                if (id == BlockID::air()) {
                    continue;
                }
                const Block& block = m_blocks.at(id);
                if (block.model.shape == BlockModel::Shape::Slab) {
                    emitSlab(out, block, origin, lx, ly, lz);
                } else if (block.model.shape == BlockModel::Shape::Stairs) {
                    emitStairs(out, block, origin, lx, ly, lz);
                } else if (block.model.shape == BlockModel::Shape::Wall) {
                    emitWall(out, block, origin, lx, ly, lz);
                } else if (block.model.shape == BlockModel::Shape::Fence) {
                    emitFence(out, block, origin, lx, ly, lz);
                } else if (block.model.shape ==
                           BlockModel::Shape::GlassPane) {
                    emitGlassPane(out, block, origin, lx, ly, lz);
                }
            }
        }
    }

    // Block faces: greedy meshing, one direction at a time (fixed order —
    // the vertex order is deterministic across runs).
    for (int f = 0; f < 6; ++f) {
        meshFaceDirection(out, *chunk, origin, kCubeFaces[f], f);
    }
    return out;
}

void ChunkMesher::meshFaceDirection(ChunkMeshData& out, const Chunk& chunk,
                                    const BlockPos& origin,
                                    const CubeFace& face, int faceIndex) const {
    const FaceSlice slice = kFaceSlices[faceIndex];

    // One 16x16 mask per slice plane. Plane p is the world plane between
    // cell p-1 and cell p along the normal axis; the +normal face lives on
    // the plane of its cell+1, the -normal face on the plane of its cell.
    for (int p = 0; p <= kChunkSize; ++p) {
        const int ca = (slice.sign > 0) ? p - 1 : p; // cell along the axis
        if (ca < 0 || ca >= kChunkSize) {
            continue;
        }

        MaskCell mask[kChunkSize][kChunkSize];
        for (int v = 0; v < kChunkSize; ++v) {
            for (int u = 0; u < kChunkSize; ++u) {
                int c[3] = {0, 0, 0};
                c[slice.axis] = ca;
                c[slice.uAxis] = u;
                c[slice.vAxis] = v;

                const BlockID id = chunk.get(c[0], c[1], c[2]);
                if (id == BlockID::air()) {
                    continue;
                }
                const Block& block = m_blocks.at(id);
                if (!block.properties.fluidName.empty()) {
                    continue; // a fluid block is stored as a fluid level
                }
                if (block.model.shape == BlockModel::Shape::Cross) {
                    continue; // cross plants emit no cube faces (the
                              // cross pass in mesh() owns their geometry)
                }
                if (block.model.shape == BlockModel::Shape::Slab ||
                    block.model.shape == BlockModel::Shape::Stairs ||
                    block.model.shape == BlockModel::Shape::Wall ||
                    block.model.shape == BlockModel::Shape::Fence ||
                    block.model.shape == BlockModel::Shape::GlassPane) {
                    continue; // partial blocks (slab / stairs / wall / fence
                              // / glass pane) — the slab pass in mesh() owns
                              // their geometry, not full-cube faces
                }
                const BlockID neighbor = m_world.getBlock(
                    origin.x + c[0] + face.dx, origin.y + c[1] + face.dy,
                    origin.z + c[2] + face.dz);
                // Phase 98: a FENCE neighbor does NOT occlude — its posts +
                // bars are thin (1/8), so a full block's face toward the
                // fence stays visible (culled only by opaque FULL neighbors;
                // the slab's half-face residual is the P85 documented limit).
                if (neighbor != BlockID::air() &&
                    m_blocks.at(neighbor).properties.opaque &&
                    m_blocks.at(neighbor).model.shape !=
                        BlockModel::Shape::Fence) {
                    continue; // fully occluded
                }

                MaskCell& cell = mask[v][u];
                cell.valid = true;
                cell.transparent = block.properties.transparent;
                // PRE-5: the cutout routing (validated to imply
                // transparent at data load). A cutout face and a blend
                // face sharing tile + light never merge (sameAs).
                cell.cutout = block.properties.cutout;
                // GEN-9: the moisture top-face variant — the +Y face of a
                // full cube carrying model.moistureTop is meshed with the
                // WET texture while the cell's soil is wet (water within
                // Chebyshev kWetWaterRadius at the same y — the crop
                // dry_slowdown rule, one predicate two consumers) and with
                // the dry `textures.top` otherwise. Derived from the live
                // fluid field at mesh time (no per-instance state): wet and
                // dry neighbors never merge (the tile differs — the greedy
                // sameAs key), and the game re-meshes tracked cells when a
                // fluid change flips their wetness.
                std::string faceTex =
                    cubeFaceTexture(block, face.textureSlot);
                if (face.textureSlot == 0 && !block.model.moistureTop.empty()) {
                    if (soilIsWet(m_world,
                                  BlockPos {origin.x + c[0], origin.y + c[1],
                                            origin.z + c[2]})) {
                        faceTex = block.model.moistureTop;
                    }
                }
                const AtlasLayout::Tile tile = m_layout.tiles.at(faceTex);
                cell.tileCol = tile.col;
                cell.tileRow = tile.row;
                // Per-face light: sample the cell the face borders
                // (block position + face normal) and bake it — the same
                // "blocky light" convention as the per-face mesher. Merging
                // below only combines faces with identical baked light.
                cell.sky = m_world.getSkyLight(origin.x + c[0] + face.dx,
                                                origin.y + c[1] + face.dy,
                                                origin.z + c[2] + face.dz);
                cell.blockLight = m_world.getBlockLight(
                    origin.x + c[0] + face.dx, origin.y + c[1] + face.dy,
                    origin.z + c[2] + face.dz);
            }
        }

        // Flood-merge: a run grows right (u) along the seed row, then down
        // (v) while the whole current width stays identical. The partition
        // is deterministic for this fixed scan order.
        bool visited[kChunkSize][kChunkSize] = {};
        for (int v = 0; v < kChunkSize; ++v) {
            for (int u = 0; u < kChunkSize; ++u) {
                const MaskCell& seed = mask[v][u];
                if (!seed.valid || visited[v][u]) {
                    continue;
                }
                int w = 1;
                while (u + w < kChunkSize && mask[v][u + w].valid &&
                       mask[v][u + w].sameAs(seed)) {
                    ++w;
                }
                int h = 1;
                while (v + h < kChunkSize) {
                    bool ok = true;
                    for (int i = 0; i < w; ++i) {
                        const MaskCell& row = mask[v + h][u + i];
                        if (!row.valid || !row.sameAs(seed)) {
                            ok = false;
                            break;
                        }
                    }
                    if (!ok) {
                        break;
                    }
                    ++h;
                }
                for (int i = 0; i < h; ++i) {
                    for (int j = 0; j < w; ++j) {
                        visited[v + i][u + j] = true;
                    }
                }
                emitMergedQuad(out, face, faceIndex, p, u, v, w, h, seed);
            }
        }
    }
}

void ChunkMesher::emitMergedQuad(ChunkMeshData& out, const CubeFace& face,
                                 int faceIndex, int p, int u0, int v0, int w,
                                 int h, const MaskCell& seed) const {
    const FaceSlice slice = kFaceSlices[faceIndex];
    const int ca = (slice.sign > 0) ? p - 1 : p;
    int c[3] = {0, 0, 0};
    c[slice.axis] = ca;
    c[slice.uAxis] = u0;
    c[slice.vAxis] = v0;

    // The seed cell's c0 corner (chunk-local), and the face's two edge
    // direction vectors from the corner table (unit, integer).
    const float cx = c[0] + face.corner[0][0];
    const float cy = c[1] + face.corner[0][1];
    const float cz = c[2] + face.corner[0][2];
    const float ewx = face.corner[1][0] - face.corner[0][0];
    const float ewy = face.corner[1][1] - face.corner[0][1];
    const float ewz = face.corner[1][2] - face.corner[0][2];
    const float ecx = face.corner[3][0] - face.corner[0][0];
    const float ecy = face.corner[3][1] - face.corner[0][1];
    const float ecz = face.corner[3][2] - face.corner[0][2];

    // Which way the mask axes point relative to the face's edge axes. The
    // run of w mask cells extends the quad by w along the c0->c1 edge (or
    // backwards), h along c0->c3 (or backwards).
    const Vec3 uVec = kAxisUnit[slice.uAxis];
    const Vec3 vVec = kAxisUnit[slice.vAxis];
    const int signA = (uVec.x * ewx + uVec.y * ewy + uVec.z * ewz) > 0.0f ? +1
                                                                         : -1;
    const int signB = (vVec.x * ecx + vVec.y * ecy + vVec.z * ecz) > 0.0f ? +1
                                                                         : -1;
    // The quad in face-local (alpha, beta) cells: alpha along the c0->c1
    // edge (alpha = 0 at c0), beta along the c0->c3 edge (beta = 0 at c0).
    // The legacy UV convention is u = alpha (in tiles), v = 1 - beta.
    const float alpha0 = (signA > 0) ? 0.0f : 1.0f - static_cast<float>(w);
    const float alpha1 = (signA > 0) ? static_cast<float>(w) : 1.0f;
    const float beta0 = (signB > 0) ? 0.0f : 1.0f - static_cast<float>(h);
    const float beta1 = (signB > 0) ? static_cast<float>(h) : 1.0f;

    // Corners in (alpha, beta): (a0,b0), (a1,b0), (a1,b1), (a0,b1) — the
    // same order type as the legacy (c0, c1, c2, c3), so the winding
    // cross(edgeW, edgeW + edgeH) stays along the face normal.
    const float corners[4][2] = {
        {alpha0, beta0}, {alpha1, beta0}, {alpha1, beta1}, {alpha0, beta1},
    };

    // PRE-5 three-way pass routing: opaque blocks -> the opaque
    // sub-mesh; transparent + cutout blocks (leaves) -> the cutout
    // sub-mesh (discard + depth write); the remaining transparent blocks
    // (glass, water) -> the translucent-blend sub-mesh.
    std::vector<MeshVertex>* verts;
    std::vector<std::uint32_t>* indices;
    if (!seed.transparent) {
        verts = &out.opaqueVertices;
        indices = &out.opaqueIndices;
    } else if (seed.cutout) {
        verts = &out.cutoutVertices;
        indices = &out.cutoutIndices;
    } else {
        verts = &out.transparentVertices;
        indices = &out.transparentIndices;
    }

    const float skyF =
        static_cast<float>(seed.sky) / static_cast<float>(kMaxLight);
    const float blockF =
        static_cast<float>(seed.blockLight) / static_cast<float>(kMaxLight);
    // Single-face quads (w = h = 1) keep the legacy atlas-space UV and
    // the shader's direct-sampling path (tiled = 0) — bit-identical to
    // the Phase 17 per-face mesh. Only actually merged quads (w * h > 1)
    // use the tiled convention: uv = the atlas tile index, uvLocal = the
    // position in tile units, and basic.frag wraps the fractional part
    // back into the tile.
    const bool tiled = (w != 1 || h != 1);
    const float legU0 = static_cast<float>(seed.tileCol) / m_layout.columns;
    const float legU1 =
        static_cast<float>(seed.tileCol + 1) / m_layout.columns;
    const float legV0 = static_cast<float>(seed.tileRow) / m_layout.rows;
    const float legV1 =
        static_cast<float>(seed.tileRow + 1) / m_layout.rows;
    // Legacy corner UVs in the (p0, p1, p2, p3) = (c0, c1, c2, c3) order.
    const float legacyUv[4][2] = {{legU0, legV1}, {legU1, legV1},
                                  {legU1, legV0}, {legU0, legV0}};

    const std::uint32_t first = static_cast<std::uint32_t>(verts->size());
    for (int i = 0; i < 4; ++i) {
        const float a = corners[i][0];
        const float b = corners[i][1];
        MeshVertex v;
        v.position = {cx + a * ewx + b * ecx, cy + a * ewy + b * ecy,
                      cz + a * ewz + b * ecz};
        v.normal = face.normal;
        if (tiled) {
            v.uv = {static_cast<float>(seed.tileCol),
                    static_cast<float>(seed.tileRow)};
            v.uvLocal = {a, 1.0f - b};
            v.tiled = 1.0f;
        } else {
            v.uv = {legacyUv[i][0], legacyUv[i][1]};
            v.tiled = 0.0f;
        }
        v.skyLight = skyF;
        v.blockLight = blockF;
        verts->push_back(v);
    }
    indices->insert(indices->end(),
                    {first, first + 1, first + 2, first, first + 2,
                     first + 3});
}

void ChunkMesher::emitFluid(ChunkMeshData& out, const Block& fluidBlock,
                            const BlockPos& origin, int lx, int ly, int lz,
                            std::uint8_t level, FluidKind kind) const {
    // The unit-cube faces scaled to the fluid height (level 8 = full cube).
    const float h =
        static_cast<float>(level) / static_cast<float>(kMaxFluidLevel);
    const int wx = origin.x + lx;
    const int wy = origin.y + ly;
    const int wz = origin.z + lz;
    for (const CubeFace& face : kCubeFaces) {
        const int nx = wx + face.dx;
        const int ny = wy + face.dy;
        const int nz = wz + face.dz;
        const BlockID neighbor = m_world.getBlock(nx, ny, nz);
        if (neighbor != BlockID::air() &&
            m_blocks.at(neighbor).properties.opaque) {
            continue; // fully occluded
        }
        // Same-kind fluid neighbor only (the other kind never hides a
        // face — the reaction pass keeps the kinds apart, but a
        // transient pre-tick state must still render both).
        const std::uint8_t neighborFluid =
            m_world.getFluidOfKind(nx, ny, nz, kind);
        // PRE-3 water ripples: the WATER top face only (lava, side and
        // bottom faces keep the flag 0 — a covered top face never
        // reaches emission, so it is unmarked by construction). The
        // vertex shader's UV-wander branch runs solely on these four
        // vertices per face (the shimmer); everything else is inert.
        const bool waterTop = (face.dy > 0) && (kind == FluidKind::Water);
        if (face.dy > 0) {
            // Top face: hidden by an equal-or-higher fluid above.
            if (neighborFluid >= level) {
                continue;
            }
        } else if (face.dy < 0) {
            // Bottom face: hidden by any fluid below.
            if (neighborFluid > 0) {
                continue;
            }
        } else if (neighborFluid >= level) {
            // Side face: hidden by an equal-or-higher fluid beside (a
            // shallower neighbor shows this cell's wall).
            continue;
        }

        const std::string& texName =
            cubeFaceTexture(fluidBlock, face.textureSlot);
        const Vec2 u0 = AtlasLayout::uvMin(m_layout, texName);
        const Vec2 u1 = AtlasLayout::uvMax(m_layout, texName);
        // Corner UVs follow the corner order: (u0,v1),(u1,v1),
        // (u1,v0),(u0,v0).
        const Vec2 faceUv[4] = {
            {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
        };
        // A fluid face is lit by its own cell (the cell behind a water face
        // is the water — its light already carries the depth attenuation).
        const std::uint8_t faceSky = m_world.getSkyLight(wx, wy, wz);
        const std::uint8_t faceBlock = m_world.getBlockLight(wx, wy, wz);
        const float skyF =
            static_cast<float>(faceSky) / static_cast<float>(kMaxLight);
        const float blockF =
            static_cast<float>(faceBlock) / static_cast<float>(kMaxLight);
        std::vector<MeshVertex>& verts = out.transparentVertices;
        std::vector<std::uint32_t>& indices = out.transparentIndices;
        const std::uint32_t first = static_cast<std::uint32_t>(verts.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {
                static_cast<float>(lx + face.corner[i][0]),
                static_cast<float>(ly + face.corner[i][1] * h),
                static_cast<float>(lz + face.corner[i][2])};
            v.normal = face.normal;
            if (waterTop) {
                // PRE-3: the WATER top face takes the TILED encoding (the
                // greedy-quad convention) so the ripple's sample window
                // wanders inside the water tile and the fragment's fract
                // wrap is seamless: a_uv = the atlas TILE INDEX (all four
                // corners carry the tile origin), a_uvLocal = the corner's
                // position in TILE UNITS (0|1 x 0|1 — the legacy corner UV
                // re-expressed from the tile origin), a_tiled = 1. The
                // a_water mark stays: it gates the wander computation in
                // the vertex stage. The legacy atlas-space faceUv and
                // (tileIndex + uvLocal) * tileSize are bit-equal (the
                // power-of-two tile fractions are exact in binary), so
                // ripples OFF renders the water top face exactly as
                // v0.26.0. Every other face (lava, sides, bottom) keeps
                // the legacy encoding untouched (tiled = 0, water = 0).
                const AtlasLayout::Tile& tile = m_layout.tiles.at(texName);
                const float tileW = 1.0f / static_cast<float>(m_layout.columns);
                const float tileH = 1.0f / static_cast<float>(m_layout.rows);
                v.uv = {static_cast<float>(tile.col),
                        static_cast<float>(tile.row)};
                v.uvLocal = {(faceUv[i].x - u0.x) / tileW,
                             (faceUv[i].y - u0.y) / tileH};
                v.tiled = 1.0f;
                v.water = 1.0f;
            } else {
                v.uv = faceUv[i];
            }
            v.skyLight = skyF;
            v.blockLight = blockF;
            verts.push_back(v);
        }
        indices.insert(indices.end(),
                       {first, first + 1, first + 2, first, first + 2,
                        first + 3});
    }
}

void ChunkMesher::emitCross(ChunkMeshData& out, const Block& block,
                            const BlockPos& origin, int lx, int ly, int lz)
    const {
    const Vec2 u0 = AtlasLayout::uvMin(m_layout, block.model.textures.cross);
    const Vec2 u1 = AtlasLayout::uvMax(m_layout, block.model.textures.cross);
    // The cell's own light (Minecraft lights cross plants with the cell's
    // light; no face shading — the normal is up, so the vertex shader's
    // face-shade table yields 1.0).
    const int wx = origin.x + lx;
    const int wy = origin.y + ly;
    const int wz = origin.z + lz;
    const float skyF =
        static_cast<float>(m_world.getSkyLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    const float blockF =
        static_cast<float>(m_world.getBlockLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    // Registry invariant: a cross shape is always transparent (validated
    // at data load). PRE-5: the quads route by the block's `cutout`
    // property — the cutout sub-mesh (leaves / flowers / wheat / torch /
    // fire / campfire) or the translucent-blend sub-mesh (a cross without
    // the field, pre-PRE-5 behavior).
    std::vector<MeshVertex>& verts =
        block.properties.cutout ? out.cutoutVertices : out.transparentVertices;
    std::vector<std::uint32_t>& indices =
        block.properties.cutout ? out.cutoutIndices : out.transparentIndices;

    // Each quad spans one cell diagonal, bottom edge first: corner order
    // (bottom-start, bottom-end, top-end, top-start), u along the
    // diagonal, v with the texture upright (bottom -> tile v1, like the
    // fluid/cube face convention).
    const float corners[2][4][3] = {
        {{0, 0, 0}, {1, 0, 1}, {1, 1, 1}, {0, 1, 0}},
        {{1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 0}},
    };
    const Vec2 uv[4] = {
        {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
    };
    // GEN-2: the stage visual — a STATEFUL cross (the sapling, the ENT-2
    // state_count pair) scales its two quads with the growth stage:
    // scale = (stage + 1) / stateCount, every corner re-anchored around
    // the CELL CENTER (the stage-0 sprout at 1/stateCount of the cell,
    // the full cell at the final stage — the Minecraft sapling read).
    // The state is the cell's per-instance byte (the sentinel falling
    // back to the block's state default, the emitStairs rule). A
    // stateless cross (wheat / torch / fire / campfire / the flowers)
    // keeps scale 1 and the EXACT pre-GEN-2 positions (the regression
    // test pins it: no stateful block in a fresh world, gate inert).
    bool scaled = false;
    float scale = 1.0f;
    if (block.stateCount() > 0) {
        int st = m_world.getBlockState(wx, wy, wz);
        if (st == Chunk::kBlockStateUnset) {
            st = block.stateDefault();
        }
        scale = static_cast<float>(st + 1) /
                static_cast<float>(block.stateCount());
        scaled = (scale != 1.0f);
    }
    for (const auto& quad : corners) {
        const std::uint32_t first = static_cast<std::uint32_t>(verts.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            if (scaled) {
                // Around the cell centre (0.5, 0.5, 0.5) in local coords.
                v.position = {
                    static_cast<float>(lx) +
                        (0.5f + (quad[i][0] - 0.5f) * scale),
                    static_cast<float>(ly) +
                        (0.5f + (quad[i][1] - 0.5f) * scale),
                    static_cast<float>(lz) +
                        (0.5f + (quad[i][2] - 0.5f) * scale)};
            } else {
                v.position = {static_cast<float>(lx + quad[i][0]),
                              static_cast<float>(ly + quad[i][1]),
                              static_cast<float>(lz + quad[i][2])};
            }
            v.normal = {0.0f, 1.0f, 0.0f}; // full-brightness face shade
            v.uv = uv[i];
            v.skyLight = skyF;
            v.blockLight = blockF;
            verts.push_back(v);
        }
        indices.insert(indices.end(),
                       {first, first + 1, first + 2, first, first + 2,
                        first + 3});
    }
}

void ChunkMesher::emitSlab(ChunkMeshData& out, const Block& block,
                           const BlockPos& origin, int lx, int ly, int lz)
    const {
    // Phase 85: the unit-cube faces scaled to the bottom half (y 0..0.5) —
    // a flat step you can stand on (the player's feet rest at y + 0.5). The
    // unit-cube corner Y is scaled by 0.5 (emitFluid's h pattern). The top
    // face is ALWAYS emitted (the cell's top half is air, so nothing above
    // occludes it within the cell); the bottom + side faces are culled by an
    // opaque neighbor (which covers their y 0..0.5 extent). Opaque sub-mesh
    // (a slab is solid, not glass).
    const float h = 0.5f;
    const int wx = origin.x + lx;
    const int wy = origin.y + ly;
    const int wz = origin.z + lz;
    for (const CubeFace& face : kCubeFaces) {
        if (face.dy < 0) {
            // Bottom + side faces: hidden by an opaque neighbor (which spans
            // the slab's y 0..0.5). The TOP face (face.dy > 0) is never
            // occluded (the cell's top half is air) — always emitted.
            const BlockID neighbor = m_world.getBlock(wx + face.dx,
                                                       wy + face.dy,
                                                       wz + face.dz);
            if (neighbor != BlockID::air() &&
                m_blocks.at(neighbor).properties.opaque) {
                continue;
            }
        }
        const Vec2 u0 = AtlasLayout::uvMin(
            m_layout, cubeFaceTexture(block, face.textureSlot));
        const Vec2 u1 = AtlasLayout::uvMax(
            m_layout, cubeFaceTexture(block, face.textureSlot));
        const Vec2 faceUv[4] = {{u0.x, u1.y}, {u1.x, u1.y},
                                {u1.x, u0.y}, {u0.x, u0.y}};
        // Per-face light: sample the cell the face borders (the blocky-light
        // convention, like the greedy cube faces).
        const std::uint8_t faceSky = m_world.getSkyLight(
            wx + face.dx, wy + face.dy, wz + face.dz);
        const std::uint8_t faceBlock = m_world.getBlockLight(
            wx + face.dx, wy + face.dy, wz + face.dz);
        const float skyF =
            static_cast<float>(faceSky) / static_cast<float>(kMaxLight);
        const float blockF =
            static_cast<float>(faceBlock) / static_cast<float>(kMaxLight);
        std::vector<MeshVertex>& verts = out.opaqueVertices;
        std::vector<std::uint32_t>& indices = out.opaqueIndices;
        const std::uint32_t first = static_cast<std::uint32_t>(verts.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {static_cast<float>(lx + face.corner[i][0]),
                          static_cast<float>(ly + face.corner[i][1] * h),
                          static_cast<float>(lz + face.corner[i][2])};
            v.normal = face.normal;
            v.uv = faceUv[i];
            v.skyLight = skyF;
            v.blockLight = blockF;
            verts.push_back(v);
        }
        indices.insert(indices.end(),
                       {first, first + 1, first + 2, first, first + 2,
                        first + 3});
    }
}

void ChunkMesher::emitWall(ChunkMeshData& out, const Block& block,
                           const BlockPos& origin, int lx, int ly, int lz)
    const {
    // Phase 95: a 1.5-block-tall pillar (the cell + a half cap above). The
    // bottom face (dy<0) is culled by an opaque neighbor below; the top cap
    // (dy>0) by an opaque neighbor above (the cap is within the cell above).
    // Phase 96 (wall connection): the SIDE faces (dy==0) against a neighbor:
    //   - a WALL neighbor: the shared face is fully internal (both pillars
    //     span y 0..1.5) -> culled (a merged wall line, no coplanar
    //     double-emit);
    //   - a FULL opaque block (solid y 0..1): its face toward the wall is
    //     already culled (the wall is opaque), so the wall's lower band
    //     (y 0..1) is internal too -> emit only the exposed cap band
    //     (y 1..1.5, above the neighbor's top), the full side texture
    //     squashed into the band (the emitSlab h pattern);
    //   - otherwise (air, partial slab/stairs, transparent glass): the lower
    //     band may be exposed (a slab covers only y 0..0.5; glass is blended
    //     in the transparent pass) -> the conservative full side (y 0..1.5).
    // Opaque sub-mesh (a wall is solid, not glass).
    const int wx = origin.x + lx;
    const int wy = origin.y + ly;
    const int wz = origin.z + lz;
    for (const CubeFace& face : kCubeFaces) {
        // The face's vertical range in cell-local units: [yBase,
        // yBase + yScale] (corner y 0/1 maps to the range ends; the bottom
        // and top-cap faces sit at a single Y, so yScale is moot for them).
        float yBase = 0.0f;
        float yScale = 1.5f;
        if (face.dy != 0) {
            // The bottom (dy<0) + top cap (dy>0) faces: hidden by an opaque
            // neighbor (which spans their extent).
            const BlockID neighbor = m_world.getBlock(wx + face.dx,
                                                      wy + face.dy,
                                                      wz + face.dz);
            if (neighbor != BlockID::air() &&
                m_blocks.at(neighbor).properties.opaque) {
                continue;
            }
        } else {
            const BlockID neighbor = m_world.getBlock(wx + face.dx, wy,
                                                      wz + face.dz);
            if (neighbor != BlockID::air()) {
                const Block& nb = m_blocks.at(neighbor);
                if (nb.model.shape == BlockModel::Shape::Wall) {
                    continue; // merged wall: the shared face is internal
                }
                if (nb.properties.opaque &&
                    nb.model.shape == BlockModel::Shape::FullCube) {
                    yBase = 1.0f; // the lower band is internal (see above)
                    yScale = 0.5f;
                }
            }
        }
        const Vec2 u0 = AtlasLayout::uvMin(
            m_layout, cubeFaceTexture(block, face.textureSlot));
        const Vec2 u1 = AtlasLayout::uvMax(
            m_layout, cubeFaceTexture(block, face.textureSlot));
        const Vec2 faceUv[4] = {{u0.x, u1.y}, {u1.x, u1.y},
                                {u1.x, u0.y}, {u0.x, u0.y}};
        const std::uint8_t faceSky = m_world.getSkyLight(
            wx + face.dx, wy + face.dy, wz + face.dz);
        const std::uint8_t faceBlock = m_world.getBlockLight(
            wx + face.dx, wy + face.dy, wz + face.dz);
        const float skyF =
            static_cast<float>(faceSky) / static_cast<float>(kMaxLight);
        const float blockF =
            static_cast<float>(faceBlock) / static_cast<float>(kMaxLight);
        std::vector<MeshVertex>& verts = out.opaqueVertices;
        std::vector<std::uint32_t>& indices = out.opaqueIndices;
        const std::uint32_t first = static_cast<std::uint32_t>(verts.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {static_cast<float>(lx + face.corner[i][0]),
                          static_cast<float>(ly + yBase +
                                             face.corner[i][1] * yScale),
                          static_cast<float>(lz + face.corner[i][2])};
            v.normal = face.normal;
            v.uv = faceUv[i];
            v.skyLight = skyF;
            v.blockLight = blockF;
            verts.push_back(v);
        }
        indices.insert(indices.end(),
                       {first, first + 1, first + 2, first, first + 2,
                        first + 3});
    }
}

void ChunkMesher::emitFence(ChunkMeshData& out, const Block& block,
                            const BlockPos& origin, int lx, int ly, int lz)
    const {
    // Phase 98: Minecraft's fence — two crossed 1/8-thick posts (y 0..1.5,
    // like the wall's solid region) + a connecting BAR toward each
    // horizontal FENCE or FULL opaque block neighbor (a fence line reads as
    // a continuous rail; a fence against a block "hooks" onto it). The bars
    // are 1/8 thick, full height, and run from the post's outer face to the
    // cell boundary; their boundary end is OPEN (the neighbor's bar meets it
    // — two coplanar end caps would double-draw) and their post end is
    // internal (meets the post face). The posts' top (y 1.5) / bottom (y 0)
    // are culled by an opaque neighbor above / below (a fence stacked on a
    // fence). Opaque sub-mesh (a fence is opaque for light, like the wall);
    // the cell's own light on every face (the cross-plant convention — the
    // posts are thin, neighbor light would flicker at line seams).
    constexpr float kHalfWidth = 0.0625f; // the post/bar half-thickness (1/8 / 2)
    constexpr float kCenter = 0.5f;
    constexpr float kTop = 1.5f;
    const float lo = kCenter - kHalfWidth;
    const float hi = kCenter + kHalfWidth;
    const int wx = origin.x + lx;
    const int wy = origin.y + ly;
    const int wz = origin.z + lz;
    const bool aboveOpaque = [&] {
        const BlockID n = m_world.getBlock(wx, wy + 1, wz);
        return n != BlockID::air() && m_blocks.at(n).properties.opaque;
    }();
    const bool belowOpaque = [&] {
        const BlockID n = m_world.getBlock(wx, wy - 1, wz);
        return n != BlockID::air() && m_blocks.at(n).properties.opaque;
    }();
    const auto barNeighbor = [&](int dx, int dz) {
        const BlockID n = m_world.getBlock(wx + dx, wy, wz + dz);
        if (n == BlockID::air()) {
            return false;
        }
        const Block& nb = m_blocks.at(n);
        return nb.model.shape == BlockModel::Shape::Fence ||
               (nb.properties.opaque &&
                nb.model.shape == BlockModel::Shape::FullCube);
    };
    const Vec2 u0 =
        AtlasLayout::uvMin(m_layout, block.model.textures.side);
    const Vec2 u1 = AtlasLayout::uvMax(m_layout, block.model.textures.side);
    const float skyF =
        static_cast<float>(m_world.getSkyLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    const float blockF =
        static_cast<float>(m_world.getBlockLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    std::vector<MeshVertex>& verts = out.opaqueVertices;
    std::vector<std::uint32_t>& indices = out.opaqueIndices;
    // One quad, kCubeFaces winding/UV convention (a/b bottom, c/d top;
    // u follows the a->b edge, v the b->c edge).
    auto emitQuad = [&](const Vec3& nrm, const Vec3& a, const Vec3& b,
                        const Vec3& c2, const Vec3& d2) {
        const std::uint32_t first = static_cast<std::uint32_t>(verts.size());
        const Vec2 uv[4] = {{u0.x, u1.y}, {u1.x, u1.y},
                            {u1.x, u0.y}, {u0.x, u0.y}};
        const Vec3 pts[4] = {a, b, c2, d2};
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {static_cast<float>(lx + pts[i].x),
                          static_cast<float>(ly + pts[i].y),
                          static_cast<float>(lz + pts[i].z)};
            v.normal = nrm;
            v.uv = uv[i];
            v.skyLight = skyF;
            v.blockLight = blockF;
            verts.push_back(v);
        }
        indices.insert(indices.end(),
                       {first, first + 1, first + 2, first, first + 2,
                        first + 3});
    };
    // An axis-aligned box (cell-local ranges), the 6 kCubeFaces windings;
    // the top / bottom faces are optional (the culling hooks).
    auto emitBox = [&](float x0, float x1, float y0, float y1, float z0,
                       float z1, bool top, bool bottom) {
        emitQuad({0.0f, 0.0f, 1.0f}, {x0, y0, z1}, {x1, y0, z1},
                 {x1, y1, z1}, {x0, y1, z1});
        emitQuad({0.0f, 0.0f, -1.0f}, {x1, y0, z0}, {x0, y0, z0},
                 {x0, y1, z0}, {x1, y1, z0});
        emitQuad({1.0f, 0.0f, 0.0f}, {x1, y0, z1}, {x1, y0, z0},
                 {x1, y1, z0}, {x1, y1, z1});
        emitQuad({-1.0f, 0.0f, 0.0f}, {x0, y0, z0}, {x0, y0, z1},
                 {x0, y1, z1}, {x0, y1, z0});
        if (top) {
            emitQuad({0.0f, 1.0f, 0.0f}, {x0, y1, z0}, {x0, y1, z1},
                     {x1, y1, z1}, {x1, y1, z0});
        }
        if (bottom) {
            emitQuad({0.0f, -1.0f, 0.0f}, {x0, y0, z1}, {x0, y0, z0},
                     {x1, y0, z0}, {x1, y0, z1});
        }
    };
    // The two crossed posts (always the full y 0..1.5; the thin side faces
    // are never coplanar with a neighbor's, so no side culling).
    emitBox(lo, hi, 0.0f, kTop, 0.0f, 1.0f, !aboveOpaque, !belowOpaque);
    emitBox(0.0f, 1.0f, 0.0f, kTop, lo, hi, !aboveOpaque, !belowOpaque);
    // A connection bar along X (x0..x1, z lo..hi, y 0..kTop): the two long
    // faces + top + bottom only — BOTH ENDS OPEN (the boundary end meets the
    // neighbor's bar end, the post end is internal against the post face;
    // capped ends would be coplanar double-draws in the opaque pass). The
    // top / bottom share the post culling.
    auto emitBarX = [&](float x0, float x1) {
        emitQuad({0.0f, 0.0f, 1.0f}, {x0, 0.0f, hi}, {x1, 0.0f, hi},
                 {x1, kTop, hi}, {x0, kTop, hi});
        emitQuad({0.0f, 0.0f, -1.0f}, {x1, 0.0f, lo}, {x0, 0.0f, lo},
                 {x0, kTop, lo}, {x1, kTop, lo});
        if (!aboveOpaque) {
            emitQuad({0.0f, 1.0f, 0.0f}, {x0, kTop, lo}, {x0, kTop, hi},
                     {x1, kTop, hi}, {x1, kTop, lo});
        }
        if (!belowOpaque) {
            emitQuad({0.0f, -1.0f, 0.0f}, {x0, 0.0f, hi}, {x0, 0.0f, lo},
                     {x1, 0.0f, lo}, {x1, 0.0f, hi});
        }
    };
    // The Z mirror of emitBarX (the long faces are the +-X sides).
    auto emitBarZ = [&](float z0, float z1) {
        emitQuad({1.0f, 0.0f, 0.0f}, {hi, 0.0f, z1}, {hi, 0.0f, z0},
                 {hi, kTop, z0}, {hi, kTop, z1});
        emitQuad({-1.0f, 0.0f, 0.0f}, {lo, 0.0f, z0}, {lo, 0.0f, z1},
                 {lo, kTop, z1}, {lo, kTop, z0});
        if (!aboveOpaque) {
            emitQuad({0.0f, 1.0f, 0.0f}, {lo, kTop, z0}, {lo, kTop, z1},
                     {hi, kTop, z1}, {hi, kTop, z0});
        }
        if (!belowOpaque) {
            emitQuad({0.0f, -1.0f, 0.0f}, {lo, 0.0f, z1}, {lo, 0.0f, z0},
                     {hi, 0.0f, z0}, {hi, 0.0f, z1});
        }
    };
    if (barNeighbor(1, 0)) {
        emitBarX(hi, 1.0f);
    }
    if (barNeighbor(-1, 0)) {
        emitBarX(0.0f, lo);
    }
    if (barNeighbor(0, 1)) {
        emitBarZ(hi, 1.0f);
    }
    if (barNeighbor(0, -1)) {
        emitBarZ(0.0f, lo);
    }
}

void ChunkMesher::emitGlassPane(ChunkMeshData& out, const Block& block,
                                const BlockPos& origin, int lx, int ly,
                                int lz) const {
    // Phase 99: a 1/16-thick 1x1 glass sheet, centered in the cell's Z
    // (z 0.46875..0.53125, y 0..1) + thin rim slivers on the four edges.
    // Blended in the TRANSPARENT sub-mesh (both sheet sides — you see the
    // pane from either end). The rim sliver toward a PANE neighbor is
    // culled (the neighbor's opposite rim is coplanar — two blended rims
    // would double-draw a darker seam where the rows meet, the transparent
    // version of the Phase 96 connection culling); toward an OPAQUE
    // neighbor the top/bottom rims are hidden too. The sheet is 1/16 thin,
    // so a pane row along X reads as one continuous sheet (each pane's
    // full-width sheet is coplanar with its neighbor's). The cell's own
    // light on every face (the cross convention).
    constexpr float kHalf = 0.03125f; // the sheet half-thickness (1/16 / 2)
    const float z0 = 0.5f - kHalf;
    const float z1 = 0.5f + kHalf;
    const int wx = origin.x + lx;
    const int wy = origin.y + ly;
    const int wz = origin.z + lz;
    const auto paneNeighbor = [&](int dx, int dy, int dz) {
        const BlockID n = m_world.getBlock(wx + dx, wy + dy, wz + dz);
        return n != BlockID::air() &&
               m_blocks.at(n).model.shape == BlockModel::Shape::GlassPane;
    };
    const auto opaqueNeighbor = [&](int dx, int dy, int dz) {
        const BlockID n = m_world.getBlock(wx + dx, wy + dy, wz + dz);
        return n != BlockID::air() && m_blocks.at(n).properties.opaque;
    };
    const Vec2 u0 =
        AtlasLayout::uvMin(m_layout, block.model.textures.side);
    const Vec2 u1 = AtlasLayout::uvMax(m_layout, block.model.textures.side);
    const float skyF =
        static_cast<float>(m_world.getSkyLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    const float blockF =
        static_cast<float>(m_world.getBlockLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    std::vector<MeshVertex>& verts = out.transparentVertices;
    std::vector<std::uint32_t>& indices = out.transparentIndices;
    auto emitQuad = [&](const Vec3& nrm, const Vec3& a, const Vec3& b,
                        const Vec3& c2, const Vec3& d2) {
        const std::uint32_t first = static_cast<std::uint32_t>(verts.size());
        const Vec2 uv[4] = {{u0.x, u1.y}, {u1.x, u1.y},
                            {u1.x, u0.y}, {u0.x, u0.y}};
        const Vec3 pts[4] = {a, b, c2, d2};
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {static_cast<float>(lx + pts[i].x),
                          static_cast<float>(ly + pts[i].y),
                          static_cast<float>(lz + pts[i].z)};
            v.normal = nrm;
            v.uv = uv[i];
            v.skyLight = skyF;
            v.blockLight = blockF;
            verts.push_back(v);
        }
        indices.insert(indices.end(),
                       {first, first + 1, first + 2, first, first + 2,
                        first + 3});
    };
    // The two sheet sides (always — double-sided, like the cross).
    emitQuad({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, z1}, {1.0f, 0.0f, z1},
             {1.0f, 1.0f, z1}, {0.0f, 1.0f, z1});
    emitQuad({0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, z0}, {0.0f, 0.0f, z0},
             {0.0f, 1.0f, z0}, {1.0f, 1.0f, z0});
    // The rim slivers (culled per neighbor, see above).
    if (!paneNeighbor(1, 0, 0)) {
        emitQuad({1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, z1}, {1.0f, 0.0f, z0},
                 {1.0f, 1.0f, z0}, {1.0f, 1.0f, z1});
    }
    if (!paneNeighbor(-1, 0, 0)) {
        emitQuad({-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, z0}, {0.0f, 0.0f, z1},
                 {0.0f, 1.0f, z1}, {0.0f, 1.0f, z0});
    }
    if (!paneNeighbor(0, 1, 0) && !opaqueNeighbor(0, 1, 0)) {
        emitQuad({0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, z0}, {0.0f, 1.0f, z1},
                 {1.0f, 1.0f, z1}, {1.0f, 1.0f, z0});
    }
    if (!paneNeighbor(0, -1, 0) && !opaqueNeighbor(0, -1, 0)) {
        emitQuad({0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, z1}, {0.0f, 0.0f, z0},
                 {1.0f, 0.0f, z0}, {1.0f, 0.0f, z1});
    }
}

void ChunkMesher::emitStairs(ChunkMeshData& out, const Block& block,
                             const BlockPos& origin, int lx, int ly, int lz)
    const {
    // Phase 87: a north-facing stair wedge — the -Z end is full height (y=1),
    // the +Z end is at the floor (y=0). Faces: a slanted TOP (always drawn —
    // the exposed slope), the -Z BACK wall (full height), the -X/+X side
    // TRIANGLES (the wedge), and the BOTTOM. The +Z front is at floor level
    // (degenerate, no face). Side/back/bottom are culled by an opaque neighbor
    // (like the slab); the slope top is always exposed.
    const int wx = origin.x + lx;
    const int wy = origin.y + ly;
    const int wz = origin.z + lz;
    // ENT-2: the per-instance state (the World's per-cell byte — the
    // project's first per-block state; for a stair, the facing). The game
    // sets it when the block is placed (to the block's state default) + on
    // rotation. Unset (the sentinel, e.g. unit tests without a game) falls
    // back to the block's state default.
    const int st = m_world.getBlockState(wx, wy, wz);
    const int f =
        (st != Chunk::kBlockStateUnset) ? st : block.stateDefault();
    const float skyF =
        static_cast<float>(m_world.getSkyLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    const float blockF =
        static_cast<float>(m_world.getBlockLight(wx, wy, wz)) /
        static_cast<float>(kMaxLight);
    const auto opaque = [&](int dx, int dy, int dz) {
        const BlockID n = m_world.getBlock(wx + dx, wy + dy, wz + dz);
        return n != BlockID::air() && m_blocks.at(n).properties.opaque;
    };
    auto emitQuad = [&](const std::array<Vec3, 4>& c, const Vec3& normal,
                        int slot) {
        const Vec2 u0 = AtlasLayout::uvMin(m_layout, cubeFaceTexture(block, slot));
        const Vec2 u1 = AtlasLayout::uvMax(m_layout, cubeFaceTexture(block, slot));
        const Vec2 uv[4] = {{u0.x, u1.y}, {u1.x, u1.y},
                            {u1.x, u0.y}, {u0.x, u0.y}};
        const std::uint32_t first =
            static_cast<std::uint32_t>(out.opaqueVertices.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {lx + c[i].x, ly + c[i].y, lz + c[i].z};
            v.normal = normal;
            v.uv = uv[i];
            v.skyLight = skyF;
            v.blockLight = blockF;
            out.opaqueVertices.push_back(v);
        }
        out.opaqueIndices.insert(out.opaqueIndices.end(),
                                 {first, first + 1, first + 2, first,
                                  first + 2, first + 3});
    };
    auto emitTri = [&](const std::array<Vec3, 3>& c, const Vec3& normal,
                       int slot) {
        const Vec2 u0 = AtlasLayout::uvMin(m_layout, cubeFaceTexture(block, slot));
        const Vec2 u1 = AtlasLayout::uvMax(m_layout, cubeFaceTexture(block, slot));
        const Vec2 uv[3] = {{u0.x, u1.y}, {u1.x, u1.y}, {u0.x, u0.y}};
        const std::uint32_t first =
            static_cast<std::uint32_t>(out.opaqueVertices.size());
        for (int i = 0; i < 3; ++i) {
            MeshVertex v;
            v.position = {lx + c[i].x, ly + c[i].y, lz + c[i].z};
            v.normal = normal;
            v.uv = uv[i];
            v.skyLight = skyF;
            v.blockLight = blockF;
            out.opaqueVertices.push_back(v);
        }
        out.opaqueIndices.insert(out.opaqueIndices.end(),
                                 {first, first + 1, first + 2});
    };

    // The NORTH wedge's 5 faces (corners + outward normal + the neighbor
    // offset that culls it; the slope top is never culled). Each is rotated
    // around Y by facing*90° for the other facings: corner points via `rot`
    // (affine, around the cell centre), normals + cull offsets via `rotDir`
    // (linear).
    const auto rot = [&](float x, float y, float z) -> Vec3 {
        switch (f) {
            case 1: return {1.0f - z, y, x};
            case 2: return {1.0f - x, y, 1.0f - z};
            case 3: return {z, y, 1.0f - x};
            default: return {x, y, z};
        }
    };
    const auto rotDir = [&](float dx, float dy, float dz) -> Vec3 {
        switch (f) {
            case 1: return {-dz, dy, dx};
            case 2: return {-dx, dy, -dz};
            case 3: return {dz, dy, -dx};
            default: return {dx, dy, dz};
        }
    };
    auto emitQuadR = [&](const std::array<Vec3, 4>& c, const Vec3& n, int slot,
                         const Vec3* cull) {
        if (cull != nullptr) {
            const Vec3 cc = rotDir(cull->x, cull->y, cull->z);
            if (opaque(cc.x, cc.y, cc.z)) {
                return;
            }
        }
        std::array<Vec3, 4> rc {};
        for (int i = 0; i < 4; ++i) {
            rc[i] = rot(c[i].x, c[i].y, c[i].z);
        }
        emitQuad(rc, rotDir(n.x, n.y, n.z), slot);
    };
    auto emitTriR = [&](const std::array<Vec3, 3>& c, const Vec3& n, int slot,
                        const Vec3* cull) {
        if (cull != nullptr) {
            const Vec3 cc = rotDir(cull->x, cull->y, cull->z);
            if (opaque(cc.x, cc.y, cc.z)) {
                return;
            }
        }
        std::array<Vec3, 3> rc {};
        for (int i = 0; i < 3; ++i) {
            rc[i] = rot(c[i].x, c[i].y, c[i].z);
        }
        emitTri(rc, rotDir(n.x, n.y, n.z), slot);
    };

    const Vec3 cullHigh {0, 0, -1};   // north high-end (-Z)
    const Vec3 cullLeft {-1, 0, 0};   // north -X side
    const Vec3 cullRight {1, 0, 0};   // north +X side
    const Vec3 cullBottom {0, -1, 0}; // north below (-Y; rotDir leaves it)
    // Slanted top (always drawn): the back edge at y=1 (z=0) to the front
    // edge at y=0 (z=1). Wound so the outward normal is up (0,1,1).
    emitQuadR(std::array<Vec3, 4>{{{0, 1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 0}}},
              {0, 1, 1}, 0, nullptr);
    // High-end wall (full height, at the -Z end for north); culled by an
    // opaque neighbor in the (rotated) high-end direction.
    emitQuadR(std::array<Vec3, 4>{{{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}},
              {0, 0, -1}, 1, &cullHigh);
    // Side triangles (the wedge) — culled by an opaque side neighbor.
    emitTriR(std::array<Vec3, 3>{{{0, 0, 0}, {0, 0, 1}, {0, 1, 0}}},
             {-1, 0, 0}, 1, &cullLeft);
    emitTriR(std::array<Vec3, 3>{{{1, 0, 0}, {1, 1, 0}, {1, 0, 1}}},
             {1, 0, 0}, 1, &cullRight);
    // Bottom — culled by an opaque below neighbor (rotDir leaves -Y unchanged).
    emitQuadR(std::array<Vec3, 4>{{{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}},
              {0, -1, 0}, 2, &cullBottom);
}

} // namespace mc
