#include "world/chunk_mesher.hpp"

#include "block/cube_face_table.hpp"
#include "core/math/vec3.hpp"
#include "world/lighting.hpp"

namespace mc {

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
    // Fluid faces are textured from the registry's fluid block
    // (properties.fluid); without one, fluid cells render nothing.
    const std::optional<BlockID> fluidId = m_blocks.fluidBlock();
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const BlockID id = chunk->get(lx, ly, lz);
                const std::uint8_t level = chunk->fluidAt(lx, ly, lz);
                if (id == BlockID::air()) {
                    if (level == 0 || fluidId == std::nullopt) {
                        continue;
                    }
                    emitFluid(out, m_blocks.at(*fluidId), origin, lx, ly, lz,
                              level);
                    continue;
                }
                if (m_blocks.at(id).properties.fluid) {
                    continue; // a fluid block is stored as a fluid level
                }
                const Block& block = m_blocks.at(id);
                std::vector<MeshVertex>* verts =
                    block.properties.transparent ? &out.transparentVertices
                                                 : &out.opaqueVertices;
                std::vector<std::uint32_t>* indices =
                    block.properties.transparent ? &out.transparentIndices
                                                 : &out.opaqueIndices;

                for (const CubeFace& face : kCubeFaces) {
                    const BlockID neighbor = m_world.getBlock(
                        origin.x + lx + face.dx, origin.y + ly + face.dy,
                        origin.z + lz + face.dz);
                    if (neighbor != BlockID::air() &&
                        m_blocks.at(neighbor).properties.opaque) {
                        continue; // fully occluded
                    }

                    const Vec2 u0 =
                        AtlasLayout::uvMin(m_layout, cubeFaceTexture(block, face.textureSlot));
                    const Vec2 u1 =
                        AtlasLayout::uvMax(m_layout, cubeFaceTexture(block, face.textureSlot));
                    // Corner UVs follow the corner order: (u0,v1),(u1,v1),
                    // (u1,v0),(u0,v0) — the Phase 2-verified tile mapping.
                    const Vec2 faceUv[4] = {
                        {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
                    };
                    // Per-face light: sample the cell the face borders
                    // (block position + face normal) and bake it into all
                    // four corners. Blocky light, like Minecraft's.
                    const std::uint8_t faceSky = m_world.getSkyLight(
                        origin.x + lx + face.dx, origin.y + ly + face.dy,
                        origin.z + lz + face.dz);
                    const std::uint8_t faceBlock = m_world.getBlockLight(
                        origin.x + lx + face.dx, origin.y + ly + face.dy,
                        origin.z + lz + face.dz);
                    const float skyF =
                        static_cast<float>(faceSky) / static_cast<float>(kMaxLight);
                    const float blockF =
                        static_cast<float>(faceBlock) / static_cast<float>(kMaxLight);
                    const std::uint32_t first =
                        static_cast<std::uint32_t>(verts->size());
                    for (int i = 0; i < 4; ++i) {
                        MeshVertex v;
                        v.position = {
                            static_cast<float>(lx + face.corner[i][0]),
                            static_cast<float>(ly + face.corner[i][1]),
                            static_cast<float>(lz + face.corner[i][2])};
                        v.normal = face.normal;
                        v.uv = faceUv[i];
                        v.skyLight = skyF;
                        v.blockLight = blockF;
                        verts->push_back(v);
                    }
                    indices->insert(indices->end(),
                                    {first, first + 1, first + 2, first,
                                     first + 2, first + 3});
                }
            }
        }
    }
    return out;
}

void ChunkMesher::emitFluid(ChunkMeshData& out, const Block& fluidBlock,
                            const BlockPos& origin, int lx, int ly, int lz,
                            std::uint8_t level) const {
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
        const std::uint8_t neighborFluid = m_world.getFluid(nx, ny, nz);
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
        // Corner UVs follow the corner order: (u0,v1),(u1,v1),(u1,v0),(u0,v0).
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

} // namespace mc
