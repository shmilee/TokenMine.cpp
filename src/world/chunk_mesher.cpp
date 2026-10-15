#include "world/chunk_mesher.hpp"

#include <array>

#include "block/cube_face_table.hpp"
#include "core/math/vec3.hpp"
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
                    (kind == FluidKind::Water) ? waterId : lavaId;
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
                    block.model.shape == BlockModel::Shape::Stairs) {
                    continue; // partial blocks (slab / stairs) — the slab
                              // pass in mesh() owns their geometry, not
                              // full-cube faces
                }
                const BlockID neighbor = m_world.getBlock(
                    origin.x + c[0] + face.dx, origin.y + c[1] + face.dy,
                    origin.z + c[2] + face.dz);
                if (neighbor != BlockID::air() &&
                    m_blocks.at(neighbor).properties.opaque) {
                    continue; // fully occluded
                }

                MaskCell& cell = mask[v][u];
                cell.valid = true;
                cell.transparent = block.properties.transparent;
                const AtlasLayout::Tile tile =
                    m_layout.tiles.at(cubeFaceTexture(block, face.textureSlot));
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

    std::vector<MeshVertex>* verts =
        seed.transparent ? &out.transparentVertices : &out.opaqueVertices;
    std::vector<std::uint32_t>* indices =
        seed.transparent ? &out.transparentIndices : &out.opaqueIndices;

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

        const Vec2 u0 = AtlasLayout::uvMin(m_layout,
                                           cubeFaceTexture(fluidBlock, face.textureSlot));
        const Vec2 u1 = AtlasLayout::uvMax(m_layout,
                                           cubeFaceTexture(fluidBlock, face.textureSlot));
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
    // at data load), so both quads go to the transparent sub-mesh.
    std::vector<MeshVertex>& verts = out.transparentVertices;
    std::vector<std::uint32_t>& indices = out.transparentIndices;

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
    for (const auto& quad : corners) {
        const std::uint32_t first = static_cast<std::uint32_t>(verts.size());
        for (int i = 0; i < 4; ++i) {
            MeshVertex v;
            v.position = {static_cast<float>(lx + quad[i][0]),
                          static_cast<float>(ly + quad[i][1]),
                          static_cast<float>(lz + quad[i][2])};
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

    // Slanted top (always drawn): the back edge at y=1 (z=0) to the front
    // edge at y=0 (z=1). Wound so the outward normal is up (0,1,1).
    emitQuad(std::array<Vec3, 4>{{{0, 1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 0}}},
             {0, 1, 1}, 0);
    // -Z back wall (full height), kCubeFaces -Z winding; culled by an opaque
    // -Z neighbor.
    if (!opaque(0, 0, -1)) {
        emitQuad(std::array<Vec3, 4>{{{1, 0, 0}, {0, 0, 0}, {0, 1, 0},
                                      {1, 1, 0}}},
                 {0, 0, -1}, 1);
    }
    // Side triangles (the wedge) — culled by an opaque side neighbor.
    if (!opaque(-1, 0, 0)) {
        emitTri(std::array<Vec3, 3>{{{0, 0, 0}, {0, 0, 1}, {0, 1, 0}}},
                {-1, 0, 0}, 1);
    }
    if (!opaque(1, 0, 0)) {
        emitTri(std::array<Vec3, 3>{{{1, 0, 0}, {1, 1, 0}, {1, 0, 1}}},
                {1, 0, 0}, 1);
    }
    // Bottom (kCubeFaces -Y winding) — culled by an opaque below neighbor.
    if (!opaque(0, -1, 0)) {
        emitQuad(std::array<Vec3, 4>{{{0, 0, 1}, {0, 0, 0}, {1, 0, 0},
                                      {1, 0, 1}}},
                 {0, -1, 0}, 2);
    }
}

} // namespace mc
