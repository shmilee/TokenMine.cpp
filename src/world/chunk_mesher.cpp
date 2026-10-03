#include "world/chunk_mesher.hpp"

#include "core/math/vec3.hpp"

namespace mc {

namespace {

// Face table for a block occupying [x, x+1] x [y, y+1] x [z, z+1].
// Corner offsets are per-axis 0/1 relative to the block's lower corner;
// corners are ordered counter-clockwise when viewed from outside the block
// (GL front faces). Winding is guarded by the FaceWinding unit test:
// cross(corner1 - corner0, corner2 - corner0) must point along the face
// normal for every face.
//
// NOTE: the Phase 1/2 unit-cube table had the +Y/-Y rows wound backwards
// (visible only from inside the cube). Its screenshot verification sampled
// side faces, which hid the bug; the Y rows here are the corrected versions.
struct Face {
    int dx, dy, dz; // neighbor block offset
    Vec3 normal;
    int corner[4][3];
    int textureSlot; // 0 = top, 1 = side, 2 = bottom
};

constexpr Face kFaces[6] = {
    {0, 1, 0, {0.0f, 1.0f, 0.0f},
     {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}, 0}, // +Y (top)
    {0, -1, 0, {0.0f, -1.0f, 0.0f},
     {{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}, 2}, // -Y (bottom)
    {0, 0, 1, {0.0f, 0.0f, 1.0f},
     {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, 1}, // +Z
    {0, 0, -1, {0.0f, 0.0f, -1.0f},
     {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, 1}, // -Z
    {1, 0, 0, {1.0f, 0.0f, 0.0f},
     {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, 1}, // +X
    {-1, 0, 0, {-1.0f, 0.0f, 0.0f},
     {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, 1}, // -X
};

const std::string& faceTexture(const Block& block, int slot) {
    switch (slot) {
        case 0:
            return block.model.textures.top;
        case 2:
            return block.model.textures.bottom;
        default:
            return block.model.textures.side;
    }
}

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
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const BlockID id = chunk->get(lx, ly, lz);
                if (id == BlockID::air()) {
                    continue;
                }
                const Block& block = m_blocks.at(id);
                std::vector<MeshVertex>* verts =
                    block.properties.transparent ? &out.transparentVertices
                                                 : &out.opaqueVertices;
                std::vector<std::uint32_t>* indices =
                    block.properties.transparent ? &out.transparentIndices
                                                 : &out.opaqueIndices;

                for (const Face& face : kFaces) {
                    const BlockID neighbor = m_world.getBlock(
                        origin.x + lx + face.dx, origin.y + ly + face.dy,
                        origin.z + lz + face.dz);
                    if (neighbor != BlockID::air() &&
                        m_blocks.at(neighbor).properties.opaque) {
                        continue; // fully occluded
                    }

                    const Vec2 u0 =
                        AtlasLayout::uvMin(m_layout, faceTexture(block, face.textureSlot));
                    const Vec2 u1 =
                        AtlasLayout::uvMax(m_layout, faceTexture(block, face.textureSlot));
                    // Corner UVs follow the corner order: (u0,v1),(u1,v1),
                    // (u1,v0),(u0,v0) — the Phase 2-verified tile mapping.
                    const Vec2 faceUv[4] = {
                        {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
                    };
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
                        v.light = 1.0f; // per-vertex light arrives in Phase 7
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

} // namespace mc
