#include "core/game.hpp"

#include <GL/glew.h>

#include <algorithm>
#include <cstddef>

#include "core/game_local.hpp"
#include "world/light_job.hpp"
#include "world/mesh_job.hpp"

namespace mc {

namespace {

// Columns posted for generation per frame. 2 = two of the four worker
// threads generate in parallel (the 99.8 multi-core fix: at 1 the pool
// idled ~75% of the time while the ring filled — generation of one
// column is ~1.6 ms, far below one frame). The per-frame lighting jobs
// (99.10: up to TWO — two new columns whose light regions are
// bit-level independent run in parallel on two workers, one box each,
// ~8.4 ms per LGT-1 step-0; a dependent pair falls back to one job,
// boxes sequential — planLightJobs, world/light_job.hpp) amortize to
// the same ~1 box per streamed column: the flush floor drops from the
// sequential 2-box ~16.7 ms to one box plus the FIFO start delay.
// kMaxStreamBacklog bounds the queue either way.
constexpr int kStreamColumnsPerFrame = 2;

// Streaming engages when the player's column is within this many columns
// (squared Euclidean, STRM-1: columnDistanceSq <= trigger², i.e. the
// player's own column + the four axis-adjacent ones — a diagonal missing
// column at distanceSq 2 does NOT engage) of a missing one inside the
// radius disk. 1 = the player must reach the edge of the loaded area; an
// idle player in the center of the 4x4 demo area (2 columns from its
// edge) never engages.
constexpr int kStreamTriggerDistance = 1;

// Unload hysteresis: a loaded column is dropped only when its squared
// distance from the player is strictly FARTHER than (view_distance + 1)²
// (STRM-1). The +1 band stops the edge columns from ping-ponging (load
// at distance R, unload at distance R) as the player's column ticks
// across a boundary.
constexpr int kUnloadHysteresis = 1;

// Re-mesh budget (PIPE-1, was kRemeshBudgetPerFrame = 8 chunks on the
// main thread): the mesher's compute moved to workers — up to this many
// COLUMNS get a snapshot mesh job per frame (postMeshJobs), nearest first
// (world/mesh_job.hpp selection). Each job meshes one whole column (8
// chunks) on a worker against its private snapshot World; the main thread
// keeps only the column-merged GL upload (updateChunkMeshes). Independent
// deque (m_streaming.meshJobs) — NOT counted against kMaxStreamBacklog's generation
// posts (the post order gen -> mesh -> light means streamChunks's
// pendingCount check runs before the mesh posts).
//
// Queue timing (why the flush stays at the per-box light floor, 4
// workers, FIFO): a fill frame posts 2 gen (~1.6 ms) + 2 mesh (<= 6 ms)
// + up to 2 light (99.10: one box each ~8.4 ms when the pair is
// independent). Worst case = 6 jobs > 4 workers: the gen jobs take two
// workers (0..1.6), the mesh jobs the other two (0..6 — hiding under
// the light), and the two light jobs (posted last) start on the first
// two freed workers (~1.6) and run IN PARALLEL (1.6..10): flush ≈
// 8.4 + 1.6 ms — the LGT-1 per-box floor plus one gen slot of FIFO
// start delay (was 13 + 1.6 with the single sequential 2-box job). The
// queue only adds latency, never a wrong result: flush() waits for
// EVERY posted job (thread_pool), and planLightJobs proves the two
// light jobs write disjoint chunks. A dependent pair posts ONE 2-box
// job instead (flush ≈ 16.7 + 1.6, the pre-99.10 behavior — the rare
// close pairs, |Δx| ≤ 3 AND |Δz| ≤ 3; the LGT-1 dominant streaming
// pairs have |Δz| 15..60 and are independent). A non-streaming frame
// posts at most 2 mesh jobs: flush <= 6 ms, and the same work no longer
// costs inside the frame.
constexpr int kMaxMeshBacklog = 2;

// Worker-pool backlog cap for the streaming posts: at most this many
// pool jobs (generation + mesh + lighting — the whole queue, 99.10: up
// to 2 of each kind per frame) may be pending (queued or running)
// before streamChunks skips its post for a frame. The per-frame flush()
// drains the whole queue, so an unbounded backlog turns into unbounded
// main-thread blocking (FPS decay the longer the disk takes to fill —
// e.g. the capped radius-64 disk is ~103k chunks). 8 = two frames of
// posts at the 4-worker pool's depth.
constexpr std::size_t kMaxStreamBacklog = 8;

// A column is the full world height: kWorldHeight / kChunkSize chunks.
constexpr int kVerticalChunksPerColumn = kWorldHeight / kChunkSize;

} // namespace

// ---------------------------------------------------------------------------
// Chunk meshing (PIPE-1): the mesher's COMPUTE runs on workers (postMeshJobs
// snapshots a column's 3x3 neighborhood into a throwaway World; the worker
// runs the UNCHANGED ChunkMesher against it — face culling across chunk
// boundaries works identically because the snapshot IS the mesher's full
// read set); integrateMeshJobs lands the results after the pool flush and
// clears the dirty flags. The game thread keeps the column-merged GL
// upload (rebuildColumnMesh — the GL context is main-thread only).
// ---------------------------------------------------------------------------

Result<void> Game::updateChunkMeshes() {
    // The merged GL mesh of every column whose chunk meshes landed this
    // frame (integrateMeshJobs) or earlier (a column can be pending across
    // frames — its chunk data exists, the upload is what lags). Phase 99.9:
    // one draw per column instead of one per chunk. Chunks without CPU
    // geometry (unlit — their mesh job waits for the light — or not meshed
    // yet) are simply absent from the merged column mesh: the same
    // pop-in visibility the old main-thread budgeted path had.
    for (const ColumnPos& col : m_columnRebuildPending) {
        MC_TRY(rebuildColumnMesh(col));
    }
    m_columnRebuildPending.clear();
    return {};
}

void Game::integrateMeshJobs() {
    if (m_streaming.meshJobs.empty()) {
        return;
    }
    // Runs right after pool->flush() (happens-before via the pool's
    // mutex) and BEFORE integrateStreamedColumns: a stream insert marks
    // face-sharing neighbors dirty, and that re-dirtying must not be
    // swallowed by a mesh integrate's clear-dirty (it re-queues the
    // column with the new neighbor in the next frame's snapshot).
    // The mesh is a pure function of the job's snapshot, so FIFO
    // integration preserves the selection order (nearest first) —
    // deterministic, frame-locked.
    std::deque<std::unique_ptr<MeshJob>> inFlight;
    for (auto& job : m_streaming.meshJobs) {
        if (!job->done) {
            inFlight.push_back(std::move(job));
            continue;
        }
        for (std::size_t cy = 0; cy < job->meshes.size(); ++cy) {
            const ChunkPos cpos{job->column.x,
                                static_cast<int>(cy), job->column.z};
            m_chunkMeshData[cpos] = std::move(job->meshes[cy]);
            // Clear the dirty flag at INTEGRATE, not post: no main-thread
            // World write can land between this frame's post and the next
            // flush (the read-only worker window), and any edit AFTER
            // this integrate re-marks the chunk and re-queues the column
            // (self-healing).
            m_world.clearChunkDirty(cpos);
        }
        m_columnRebuildPending.insert(job->column);
        m_streaming.meshPending.erase(job->column);
        MC_LOG_DEBUG("Meshed column (" + std::to_string(job->column.x) +
                     ", " + std::to_string(job->column.z) + ") on worker");
    }
    m_streaming.meshJobs = std::move(inFlight);
}

Result<void> Game::rebuildColumnMesh(const ColumnPos& col) {
    // Concatenate the column's chunk geometries. Each chunk's Y offset
    // (its layer in the column) is baked into the vertex positions; the
    // X/Z column origin stays in the model matrix (columnModel) — baking
    // it there too would double-apply the translation. Chunks without
    // CPU geometry (unlit, or not meshed yet) are simply absent — the same
    // visibility the old per-chunk loop had.
    std::vector<MeshVertex> opaqueVerts;
    std::vector<std::uint32_t> opaqueIdx;
    // PRE-5: the third (cutout) bucket, merged like the other two.
    std::vector<MeshVertex> cutoutVerts;
    std::vector<std::uint32_t> cutoutIdx;
    std::vector<MeshVertex> transparentVerts;
    std::vector<std::uint32_t> transparentIdx;
    for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
        const ChunkPos cp{col.x, cy, col.z};
        const auto it = m_chunkMeshData.find(cp);
        if (it == m_chunkMeshData.end()) {
            continue;
        }
        const ChunkMeshData& d = it->second;
        const float oy = static_cast<float>(cy * kChunkSize);
        if (!d.opaqueEmpty()) {
            const std::uint32_t base =
                static_cast<std::uint32_t>(opaqueVerts.size());
            opaqueVerts.reserve(opaqueVerts.size() + d.opaqueVertices.size());
            for (const MeshVertex& v : d.opaqueVertices) {
                MeshVertex w = v;
                w.position.y += oy;
                opaqueVerts.push_back(std::move(w));
            }
            opaqueIdx.reserve(opaqueIdx.size() + d.opaqueIndices.size());
            for (const std::uint32_t i : d.opaqueIndices) {
                opaqueIdx.push_back(i + base);
            }
        }
        if (!d.cutoutEmpty()) {
            const std::uint32_t base =
                static_cast<std::uint32_t>(cutoutVerts.size());
            cutoutVerts.reserve(cutoutVerts.size() + d.cutoutVertices.size());
            for (const MeshVertex& v : d.cutoutVertices) {
                MeshVertex w = v;
                w.position.y += oy;
                cutoutVerts.push_back(std::move(w));
            }
            cutoutIdx.reserve(cutoutIdx.size() + d.cutoutIndices.size());
            for (const std::uint32_t i : d.cutoutIndices) {
                cutoutIdx.push_back(i + base);
            }
        }
        if (!d.transparentEmpty()) {
            const std::uint32_t base =
                static_cast<std::uint32_t>(transparentVerts.size());
            transparentVerts.reserve(
                transparentVerts.size() + d.transparentVertices.size());
            for (const MeshVertex& v : d.transparentVertices) {
                MeshVertex w = v;
                w.position.y += oy;
                transparentVerts.push_back(std::move(w));
            }
            transparentIdx.reserve(
                transparentIdx.size() + d.transparentIndices.size());
            for (const std::uint32_t i : d.transparentIndices) {
                transparentIdx.push_back(i + base);
            }
        }
    }
    ColumnMesh& cm = m_columnMeshes[col];
    cm.opaque.reset();
    cm.cutout.reset();
    cm.transparent.reset();
    if (!opaqueIdx.empty()) {
        cm.opaque.emplace();
        auto r = cm.opaque->upload(opaqueVerts, opaqueIdx);
        MC_TRY(r);
    }
    if (!cutoutIdx.empty()) {
        cm.cutout.emplace();
        auto r = cm.cutout->upload(cutoutVerts, cutoutIdx);
        MC_TRY(r);
    }
    if (!transparentIdx.empty()) {
        cm.transparent.emplace();
        auto r = cm.transparent->upload(transparentVerts, transparentIdx);
        MC_TRY(r);
    }
    return {};
}
// ---------------------------------------------------------------------------
// Chunk streaming (Phase 14): worker-generated columns around the player
// ---------------------------------------------------------------------------

void Game::integrateStreamedColumns() {
    if (m_streaming.streamJobs.empty()) {
        return;
    }
    // Runs right after pool->flush(): completed jobs are fully published
    // (happens-before via the pool's mutex). insertChunk marks the chunk
    // and its face-sharing neighbors dirty, so updateChunkMeshes (later
    // this frame) re-meshes them with correct boundary culling.
    std::deque<std::unique_ptr<StreamJob>> inFlight;
    for (auto& job : m_streaming.streamJobs) {
        if (!job->done) {
            inFlight.push_back(std::move(job));
            continue;
        }
        for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
            const ChunkPos cpos {job->column.x, cy, job->column.z};
            m_world.insertChunk(cpos, std::move(job->chunks[cy]));
            // Burn-pass gate (Phase 99.8): a freshly generated column can
            // carry flamables (wood, leaves, grass) — note the chunks.
            const Chunk* c = m_world.findChunk(cpos);
            if (c != nullptr) {
                const BlockID* blocks = c->data();
                for (int i = 0; i < Chunk::kVolume; ++i) {
                    if (m_blocks.at(blocks[i]).properties.flamable) {
                        m_flammableChunks.insert(cpos);
                        break;
                    }
                }
            }
        }
        m_streaming.streamPending.erase(job->column);
        m_streaming.loadedColumns.insert(job->column);
        // New terrain needs light propagation (the old m_litChunks full
        // scan detected it; the explicit column set replaces it) — and
        // stays UNMESHED until that light lands (m_streaming.unlitColumns: the
        // chunk meshes once, lit, instead of dark-then-re-meshed).
        m_lightPendingColumns.insert(job->column);
        m_streaming.unlitColumns.insert(job->column);
        // STRM-2: re-apply this column's retained diff (the session
        // changes it held before an earlier unload — the pure-fluid
        // churn, the fire burn, the water-lava reaction: the writes that
        // bypass the game's setBlock choke point and so never earned the
        // m_streaming.editedColumns protection). BLOCKS FIRST, then fluids — the
        // applyWorldEdits order (a block displaces the cell's fluid, so a
        // re-applied placement re-drains the generated fluid before the
        // fluid edits land; a level-0 fluid edit on an already-dry cell
        // is the expected no-op). The observer re-journals every write:
        // the column is in the journal again and out of the retained
        // map — the one-of invariant, and the next unload snapshots it
        // again (the self-similar loop). Deliberately NOT inserted into
        // m_streaming.editedColumns: that would keep the column's chunks resident
        // for the whole session (the memory regression this feature
        // replaces) — a fluid-only column must stay unloadable.
        // Lighting: the column just joined m_lightPendingColumns /
        // m_streaming.unlitColumns above (new terrain, the forced propagation), so
        // this frame's postLightJobs covers the re-applied cells and the
        // meshes form once, lit (no lit-pop-in, the 99.4 discipline).
        if (auto rit = m_streaming.retainedColumns.find(job->column);
            rit != m_streaming.retainedColumns.end()) {
            for (const SavedBlockEdit& e : rit->second.blocks) {
                const BlockID id = m_blocks.idForName(e.blockName);
                if (!id.valid()) {
                    // Structurally unreachable (same session, same
                    // registry as the snapshot — the name resolved when
                    // the diff was produced): name it and skip the ONE
                    // edit — never crash, never drop the column
                    // silently.
                    MC_LOG_ERROR("STRM-2: retained block edit at (" +
                                 std::to_string(e.pos.x) + ", " +
                                 std::to_string(e.pos.y) + ", " +
                                 std::to_string(e.pos.z) +
                                 ") names unknown block '" + e.blockName +
                                 "' — skipped");
                    continue;
                }
                // The cell's y was valid when the write was journal-
                // recorded, and the id is valid: setBlock cannot fail.
                m_world.setBlock(e.pos.x, e.pos.y, e.pos.z, id);
            }
            for (const SavedFluidEdit& e : rit->second.fluids) {
                // A positive level targets the cell the block pass left
                // (air — a cell holds fluid XOR a block), so setFluid
                // cannot fail for it; the level-0 drain on an already-
                // dry cell is the expected false no-op.
                m_world.setFluid(e.pos.x, e.pos.y, e.pos.z, e.kind, e.level);
            }
            m_streaming.retainedColumns.erase(rit);
        }
        // Per-column detail at DEBUG (a radius-80 session logs 25921 of
        // these lines; the periodic INFO progress line in streamChunks is
        // the human-readable signal).
        MC_LOG_DEBUG("Streamed column (" + std::to_string(job->column.x) +
                     ", " + std::to_string(job->column.z) +
                     ") integrated (" + std::to_string(m_world.chunkCount()) +
                     " chunks loaded)");
    }
    m_streaming.streamJobs = std::move(inFlight);
}

void Game::collectLitColumns() {
    if (m_streaming.lightJobs.empty()) {
        return;
    }
    // Runs right after pool->flush(): done jobs have finished writing
    // light (happens-before via the pool's mutex), so their columns can
    // be meshed this frame with the propagated light. Undone jobs stay
    // in the deque (their columns stay unlit/unmeshed).
    std::deque<std::unique_ptr<LightJob>> inFlight;
    for (auto& job : m_streaming.lightJobs) {
        if (!job->done) {
            inFlight.push_back(std::move(job));
            continue;
        }
        for (const ColumnPos& col : job->columns) {
            if (job->fluid) {
                // 99.11: the fluid catch-up's bookkeeping. The column
                // already left m_fluidLightPending at POST time (the
                // budget must not re-take an in-flight column) — and
                // this frame's fluid tick runs AFTER this reap, so no
                // re-churn can have re-added it: the erase is the
                // contract belt (a processed column never lingers).
                // Fluid columns are NOT unlit columns (they were lit
                // long before the fluid touched them): m_streaming.unlitColumns —
                // the streaming lit-gate — is not touched.
                m_fluidLightPending.erase(col);
            } else {
                m_streaming.unlitColumns.erase(col);
            }
        }
        // 99.10: apply the workers' net-changed chunks to the World's
        // dirty set HERE (the main-thread-writer window — no worker is
        // in flight): the jobs collected them job-locally, because two
        // parallel light jobs would race on the shared set. 99.11:
        // markChunkDirty silently skips a missing chunk (World
        // contract) — the defensive path for a fluid job whose column
        // was unloaded in the meantime (timing keeps it unreachable:
        // the flush barrier completes every job before this frame's
        // unloadFarColumns runs).
        for (const ChunkPos& pos : job->dirtyChunks) {
            m_world.markChunkDirty(pos);
        }
    }
    m_streaming.lightJobs = std::move(inFlight);
}

void Game::unloadFarColumns() {
    if (!m_player) {
        return;
    }
    // Safe point: the pool flushed at the top of update and the posts run
    // at its end — no worker touches the World while this erases (the
    // main-thread-writer window, same as integrateStreamedColumns).
    const Vec3 p = m_player->position();
    const ColumnPos player{
        floorDiv(static_cast<int>(std::floor(p.x)), kChunkSize),
        floorDiv(static_cast<int>(std::floor(p.z)), kChunkSize)};
    const int keepRadius = std::min(m_streaming.streamRadius, kStreamCapRadius);
    const int keepDist = keepRadius + kUnloadHysteresis;
    // The unload predicate is squared Euclidean (STRM-1): a column
    // survives while its distanceSq is <= (keepRadius + 1)² — the +1
    // hysteresis band around the capped disk.
    const std::int64_t keepDistSq =
        static_cast<std::int64_t>(keepDist) * keepDist;
    // The per-block state maps (furnace/campfire/table/crop/beacon/fire)
    // are keyed by BlockPos: their COLUMNS must survive (the save records
    // the states and cross-checks them against the block edits — an
    // unloaded column would break the load). The maps are player-scale
    // (small).
    std::set<ColumnPos> protectedColumns = m_streaming.editedColumns;
    protectedColumns.insert(m_streaming.streamPending.begin(), m_streaming.streamPending.end());
    // PIPE-1: in-flight MESH columns too (same paradigm — their
    // integrate must find the column still loaded; unloading it would
    // strand the 8 ChunkMeshData outputs in m_chunkMeshData).
    protectedColumns.insert(m_streaming.meshPending.begin(), m_streaming.meshPending.end());
    const auto protectStates = [this, &protectedColumns](
                                   const auto& states) {
        for (const auto& [pos, st] : states) {
            protectedColumns.insert({
                floorDiv(pos.x, kChunkSize), floorDiv(pos.z, kChunkSize)});
        }
    };
    protectStates(m_states.furnaces);
    protectStates(m_states.campfires);
    protectStates(m_states.tables);
    protectStates(m_states.crops);
    protectStates(m_states.saplings);
    protectStates(m_states.beacons);
    protectStates(m_states.fires);

    std::vector<ChunkPos> toErase;
    // The loaded COLUMNS (the incremental mirror), not a full chunk scan:
    // a full-ring world is ~9k-200k chunks, the column set is ~2k-26k.
    for (const ColumnPos& col : m_streaming.loadedColumns) {
        if (columnDistanceSq(player, col) <= keepDistSq) {
            continue; // inside the loaded disk (1-column hysteresis band)
        }
        // The 4x4 demo area is protected: respawn teleports to the spawn
        // point inside it (an unloaded spawn column would read void).
        if (col.x >= kDemoChunkMin && col.x <= kDemoChunkMax &&
            col.z >= kDemoChunkMin && col.z <= kDemoChunkMax) {
            continue;
        }
        if (protectedColumns.count(col) > 0) {
            continue; // player edits / tracked state / in-flight job
        }
        for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
            toErase.push_back({col.x, cy, col.z});
        }
    }
    if (toErase.empty()) {
        return;
    }
    // The unloaded COLUMNS (toErase holds all eight chunks of each — the
    // merged column mesh and every member's CPU geometry go with them).
    std::set<ColumnPos> columns;
    for (const ChunkPos& pos : toErase) {
        columns.insert({pos.x, pos.z});
    }
    // STRM-2: snapshot the journal diff (live != fresh, restricted to the
    // column's journal cells) of every unloaded column BEFORE its chunks
    // are erased — the snapshot reads the live chunks. A column with no
    // journal entries in EITHER table costs two map finds and nothing
    // else (the structural fast path: most unloaded columns are pristine
    // generated terrain); an all-revert column yields an empty diff and
    // is not stored. On re-stream the diff is re-applied
    // (integrateStreamedColumns) and merged into the exit save
    // (collectSave) — the one-of invariant with the journal below is
    // established HERE: the snapshot precedes the eviction.
    for (const ColumnPos& col : columns) {
        const auto jb = m_streaming.saveJournal.blocks.find(col);
        const auto jf = m_streaming.saveJournal.fluids.find(col);
        if (jb == m_streaming.saveJournal.blocks.end() &&
            jf == m_streaming.saveJournal.fluids.end()) {
            continue;
        }
        RetainedColumnDiff diff = retainColumnDiff(
            m_world, makeChunkGenerator(), m_blocks, col,
            jb == m_streaming.saveJournal.blocks.end() ? nullptr : &jb->second,
            jf == m_streaming.saveJournal.fluids.end() ? nullptr : &jf->second);
        if (!diff.blocks.empty() || !diff.fluids.empty()) {
            m_streaming.retainedColumns.emplace(col, std::move(diff));
        }
    }
    for (const ChunkPos& pos : toErase) {
        m_chunkMeshData.erase(pos);
        m_world.eraseChunk(pos); // marks the boundary neighbors dirty
        m_flammableChunks.erase(pos);
    }
    for (const ColumnPos& col : columns) {
        m_columnMeshes.erase(col); // frees the GL buffers (Mesh destructor)
        m_columnRebuildPending.erase(col);
        // PIPE-2 / STRM-2: the journal entries of an unloaded column go
        // with it — the session's diff they carried was just snapshotted
        // into m_streaming.retainedColumns above (when non-empty), so nothing is
        // lost: the column re-enters as FRESH streamed terrain plus its
        // retained diff.
        m_streaming.saveJournal.evictColumn(col);
    }
    for (const ChunkPos& pos : toErase) {
        m_streaming.loadedColumns.erase({pos.x, pos.z});
        // Defensive: an unlit column cannot be unloaded (it is either
        // inside the ring, edited, or in flight — all protected above),
        // but the set must not leak entries for dead columns.
        m_streaming.unlitColumns.erase({pos.x, pos.z});
        // 99.11: the fluid catch-up pending set must not accumulate
        // entries for dead columns either (a coast tick's changed cell
        // can outlive its column's unload window while the budget lags
        // behind the churn): evict them here, the same no-leak belt. An
        // in-flight fluid job of an unloaded column cannot exist at
        // this point (the flush barrier completed it before the reap
        // above); its reap applied dirtyOut while the chunks still
        // existed, and markChunkDirty skips any that did not.
        m_fluidLightPending.erase({pos.x, pos.z});
    }
    m_streaming.unloadedColumnsTotal +=
        static_cast<std::size_t>(toErase.size() / kVerticalChunksPerColumn);
    // Per-event detail at DEBUG (walking at radius 80 produces one of
    // these every ~4 s; the periodic INFO progress line carries the
    // count).
    MC_LOG_DEBUG("Unloaded " +
                 std::to_string(toErase.size() / kVerticalChunksPerColumn) +
                 " far columns (" + std::to_string(m_world.chunkCount()) +
                 " chunks loaded)");
}

void Game::streamChunks() {
    if (!m_streaming.pool || !m_generator || !m_player) {
        return;
    }
    // While the player is dead the body is frozen and the death screen
    // owns the input: stop requesting new columns. The radius disk
    // around the corpse would otherwise keep filling to the capped view
    // distance (the radius-64 disk holds 12853 columns ≈ 103k chunks:
    // hours of streaming and several GB of allocations behind the death
    // veil). In-flight jobs
    // (at most kStreamColumnsPerFrame) still drain through the
    // post->flush window and get lit by postLightJobs as usual, and the
    // preserved hysteresis state resumes the fill on respawn (R).
    if (m_player->isDead()) {
        return;
    }
    // The player's column (bottom-center position).
    const Vec3 p = m_player->position();
    const ColumnPos player{
        floorDiv(static_cast<int>(std::floor(p.x)), kChunkSize),
        floorDiv(static_cast<int>(std::floor(p.z)), kChunkSize)};

    // The loaded columns (the incremental mirror built by buildScene /
    // integrate / unload / first-touch creation — the old code rebuilt
    // this from a full per-frame chunk scan + sort).
    const std::vector<ColumnPos> loaded(m_streaming.loadedColumns.begin(),
                                        m_streaming.loadedColumns.end());

    // Hysteresis (see chunk_streaming): engage when the player reaches
    // the edge of the loaded area; while engaged, keep filling the full
    // radius disk (an exploring player ends up with the capped
    // view_distance of terrain around them); disengage only when the
    // disk around the player is fully loaded (nearestMissingDistanceSq
    // returns the (radius + 1)² sentinel). A never-engaged idle player
    // in the center of the loaded area stays job-free (keeps headless
    // baselines byte-reproducible). The effective radius is the config
    // clamped to the far-plane cap (kStreamCapRadius); m_streaming.streamRadius
    // itself keeps the raw config value for the logs and the F3 overlay.
    const int radius = std::min(m_streaming.streamRadius, kStreamCapRadius);
    const std::int64_t nearestMissingSq =
        nearestMissingDistanceSq(player, loaded, radius);
    if (!m_streaming.streamingEngaged) {
        const std::int64_t triggerSq = static_cast<std::int64_t>(
            kStreamTriggerDistance) * kStreamTriggerDistance;
        if (nearestMissingSq > triggerSq) {
            return;
        }
        m_streaming.streamingEngaged = true;
        MC_LOG_INFO("Chunk streaming engaged (radius " +
                    std::to_string(m_streaming.streamRadius) + " columns)");
    } else if (nearestMissingSq >= missingSentinelDistanceSq(radius)) {
        m_streaming.streamingEngaged = false; // the radius disk is fully loaded
        // One explicit completion line: after this, streaming is quiet
        // (no per-frame work) until the player reaches the loaded edge
        // again — silence in the log = the disk is full, not a stall.
        MC_LOG_INFO("Streaming complete: " +
                    std::to_string(m_streaming.loadedColumns.size()) + "/" +
                    std::to_string(diskColumnCount(radius)) +
                    " columns (" + std::to_string(m_world.chunkCount()) +
                    " chunks, " +
                    std::to_string(m_streaming.unloadedColumnsTotal) +
                    " unloaded) — quiet until the player reaches the "
                    "loaded edge");
    }

    // Backlog cap: the per-frame flush() at the top of update waits for
    // EVERY in-flight pool job, so an unbounded queue makes the main
    // thread block longer and longer (FPS decay while the ring fills).
    // Generation is the heavy post; the per-frame lighting jobs (<= 2,
    // 99.10) stay uncapped (one box each — cheap per integrated column).
    // Skipping the post this frame (not the column) is safe: the loop
    // re-runs next frame and m_streaming.streamPending only holds actually-posted
    // columns.
    if (m_streaming.pool->pendingCount() >= kMaxStreamBacklog) {
        return;
    }
    int posted = 0;
    const std::vector<ColumnPos> missing =
        missingColumns(player, loaded, radius);
    for (const ColumnPos& col : missing) {
        if (posted >= kStreamColumnsPerFrame) {
            break;
        }
        if (m_streaming.streamPending.count(col) > 0) {
            continue; // a generation job for this column is in flight
        }
        auto job = std::make_unique<StreamJob>();
        job->column = col;
        job->chunks = std::make_unique<Chunk[]>(kVerticalChunksPerColumn);
        m_streaming.streamPending.insert(col);
        StreamJob* rawJob = job.get();
        m_streaming.streamJobs.push_back(std::move(job));
        // The worker fills the job's own scratch chunks with the pure
        // generator (no World access, no shared mutable state — the
        // generator's members are immutable after create()).
        m_streaming.pool->post([this, rawJob] {
            for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
                m_generator->generateChunk(rawJob->chunks[cy],
                                           {rawJob->column.x, cy,
                                            rawJob->column.z});
            }
            rawJob->done = true;
        });
        ++posted;
    }
    // Periodic INFO progress (the per-column lines are DEBUG): every
    // 300 frames while streaming is engaged — the human-readable
    // "is it still loading / did it stop" signal for a long session.
    // Frame-based (deterministic cadence).
    if (m_frameCount % 300 == 0) {
        MC_LOG_INFO("Streaming: " +
                    std::to_string(m_streaming.loadedColumns.size()) + "/" +
                    std::to_string(diskColumnCount(radius)) +
                    " columns (" + std::to_string(m_world.chunkCount()) +
                    " chunks, " +
                    std::to_string(m_streaming.unloadedColumnsTotal) +
                    " unloaded since session start)");
    }
}

void Game::postLightJobs() {
    if (!m_streaming.pool) {
        return;
    }
    // Columns that appeared since the last post (integrated at the top of
    // update, or first-touch completed by the setBlock choke point).
    // Everything else is already covered by the lighting pipeline (init/
    // load propagateAll + local updates). The old code found these by a
    // full per-frame chunk scan against m_litChunks; the explicit pending
    // set replaces it. Columns are complete (the ensureColumn invariant),
    // so each box is the full column.
    if (m_lightPendingColumns.empty()) {
        return;
    }
    // 99.10 (hard rule 7, revised): up to TWO lighting jobs per frame —
    // and only when the two pending columns' light regions are bit-level
    // independent (planLightJobs: the WRITE footprint is the column box
    // ±kLightUpdateRadius = ±1 column, and the block-light BFS READS one
    // more column ring as its boundary seed, so a column gap >=
    // kLightParallelColumnGap on either axis separates write from write
    // AND write from read). Two independent jobs run in parallel on two
    // workers and touch DISJOINT chunks only — the post->flush window
    // contract holds (the main thread only reads the World in it, and no
    // other job writes the live World after the mesh snapshots). The
    // confluence half: independent regions cannot influence each other
    // in either order, so the final light is a pure function of the
    // pre-insertion world regardless of which job finishes first.
    //
    // A dependent pair (or 3+ pending columns) posts ONE job running its
    // boxes sequentially in set order — the pre-99.10 behavior,
    // bit-identical (the latent sky-rescan clobber of OVERLAPPING
    // regions is unchanged by design — LGT-1 disposition, its fix left
    // for 100+).
    //
    // The jobs collect their net-changed chunk positions in
    // LightJob::dirtyChunks instead of calling World::markChunkDirty:
    // two parallel markChunkDirty calls would race on the World's shared
    // dirty set; collectLitColumns applies them at the next flush (the
    // main-thread-writer window). The job also carries its COLUMNS and a
    // done flag: at the next flush, collectLitColumns drops them from
    // m_streaming.unlitColumns so their chunks get a mesh job once, lit (PIPE-1
    // selection rule).
    const LightJobPlan plan = planLightJobs(m_lightPendingColumns);
    m_lightPendingColumns.clear();
    for (const std::vector<ColumnPos>& columns : plan.jobs) {
        auto job = std::make_unique<LightJob>();
        job->columns = std::set<ColumnPos>(columns.begin(), columns.end());
        job->boxes.reserve(columns.size());
        for (const ColumnPos& col : columns) {
            job->boxes.push_back(columnLightBox(col));
        }
        LightJob* rawJob = job.get();
        m_streaming.lightJobs.push_back(std::move(job));
        m_streaming.pool->post([this, rawJob] {
            for (const auto& box : rawJob->boxes) {
                // newTerrain = true: the boxes cover freshly inserted
                // columns — the sky propagation must run even when the
                // rescan writes nothing (a column born dark still
                // receives light from its lit neighbors). dirtyOut: the
                // net change is collected job-locally (see above).
                Lighting::updateRegion(m_world, m_blocks, box[0], box[1],
                                       true, &rawJob->dirtyChunks);
            }
            rawJob->done = true;
        });
    }
}

void Game::postFluidLightJobs() {
    if (!m_streaming.pool) {
        return;
    }
    if (m_fluidLightPending.empty()) {
        return; // a settled world: the fast exit (structural immunity)
    }
    // The streaming job columns of THIS window (postLightJobs ran
    // earlier in the frame; the previous window's jobs — of either kind
    // — were reaped at the top after the flush barrier, so nothing is in
    // flight when this runs). Window exclusivity (see the header
    // comment + the 99.11 note in world/light_job.hpp): a fluid column
    // whose light region is not lightRegionsIndependent of one of these
    // columns is skipped this frame — it stays pending and is retried
    // next frame. The 99.10 parallel safety is per-plan; a shared
    // window would let two workers write one chunk's light arrays.
    std::set<ColumnPos> streaming;
    for (const std::unique_ptr<LightJob>& job : m_streaming.lightJobs) {
        if (!job->fluid) {
            streaming.insert(job->columns.begin(), job->columns.end());
        }
    }
    const LightJobPlan plan =
        planFluidLightJobs(m_fluidLightPending, streaming);
    if (plan.jobs.empty()) {
        return; // every candidate was blocked: nothing to post
    }
    // The taken columns leave the pending set at POST time — before the
    // jobs run — so the budget never re-takes an in-flight column. A
    // re-churn of the column (the next fluid tick) re-adds it after the
    // job lands: a new world state, a new re-light, correctly sequenced
    // by the flush barrier.
    for (const std::vector<ColumnPos>& columns : plan.jobs) {
        for (const ColumnPos& col : columns) {
            m_fluidLightPending.erase(col);
        }
    }
    for (const std::vector<ColumnPos>& columns : plan.jobs) {
        auto job = std::make_unique<LightJob>();
        job->fluid = true;
        job->columns = std::set<ColumnPos>(columns.begin(), columns.end());
        job->boxes.reserve(columns.size());
        for (const ColumnPos& col : columns) {
            job->boxes.push_back(columnLightBox(col));
        }
        LightJob* rawJob = job.get();
        m_streaming.lightJobs.push_back(std::move(job));
        m_streaming.pool->post([this, rawJob] {
            for (const auto& box : rawJob->boxes) {
                // newTerrain = true: unlike the old synchronous fluid
                // re-light (default false), the catch-up forces the sky
                // propagation even when the rescan writes nothing — the
                // 99.9 no-op skip would strand a fluid-opened horizontal
                // light path dark (a cave film's vertical values are
                // unchanged while its attenuation path changes). The
                // streaming jobs pay the same cost (99.10).
                // dirtyOut: job-local net-changed collection — the fluid
                // columns' chunks are re-meshed with the new light at
                // the next flush (collectLitColumns), the 99.4
                // lit-pop-in discipline.
                Lighting::updateRegion(m_world, m_blocks, box[0], box[1],
                                       true, &rawJob->dirtyChunks);
            }
            rawJob->done = true;
        });
    }
}

void Game::postMeshJobs() {
    // PIPE-1: worker meshing. Runs in update AFTER streamChunks (gen
    // posts) and BEFORE the light posts (99.11: postLightJobs THEN
    // postFluidLightJobs) — the post order gen -> mesh -> light is the
    // contract: after this function's posts, the ONLY jobs that write
    // the live World in the coming window are this frame's lighting
    // jobs (streaming + fluid catch-up), and nextMeshColumns's
    // exclusions (unlit + its footprint) keep every snapshot's light
    // final for the streaming ones; a fluid re-light of a column whose
    // mesh was snapshotted this frame re-marks the chunk dirty through
    // its dirtyOut at the next flush, so the mesh re-queues after it
    // (self-healing — one frame of stale light, the 99.4 pattern). The
    // snapshot is taken NOW — after every main-thread World write of
    // this frame (interaction, fluids, fire, growth), before the
    // player's next move — so it is the frame's final block/fluid
    // state (a pure function of the World at this point).
    if (!m_streaming.pool) {
        return;
    }
    const std::set<ChunkPos>& dirtySet = m_world.dirtyChunks();
    if (dirtySet.empty()) {
        return;
    }
    // The column projection of the dirty set (the set is sorted -> the
    // projection is deterministic before the distance sort).
    std::set<ColumnPos> dirtyColumns;
    for (const ChunkPos& pos : dirtySet) {
        dirtyColumns.insert({pos.x, pos.z});
    }
    if (dirtyColumns.empty()) {
        return;
    }
    const Vec3 p =
        m_player ? m_player->position() : Vec3{0.0, 0.0, 0.0};
    const ColumnPos player{
        floorDiv(static_cast<int>(std::floor(p.x)), kChunkSize),
        floorDiv(static_cast<int>(std::floor(p.z)), kChunkSize)};
    const std::vector<ColumnPos> chosen = nextMeshColumns(
        dirtyColumns, player, m_streaming.unlitColumns, m_lightPendingColumns,
        m_streaming.meshPending, kMaxMeshBacklog);
    for (const ColumnPos& col : chosen) {
        auto job = std::make_unique<MeshJob>();
        job->column = col;
        job->meshes.resize(kVerticalChunksPerColumn);
        // The snapshot: the column's 3x3 column neighborhood, all layers
        // (the mesher's full read set — its faces cull and bake light
        // against at most the 6 neighbor columns' cells). Missing
        // neighbors stay missing: the snapshot World's void reads match
        // the live world's bit-for-bit (the same World class).
        // insertSnapshotChunk carries the cell content + light + fluid +
        // stair facing with no live-World bookkeeping (the snapshot's
        // indexes are never consulted).
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                for (int cy = 0; cy < kVerticalChunksPerColumn; ++cy) {
                    const ChunkPos cp{col.x + dx, cy, col.z + dz};
                    const Chunk* src = m_world.findChunk(cp);
                    if (src != nullptr) {
                        job->snap.insertSnapshotChunk(cp, *src);
                    }
                }
            }
        }
        m_streaming.meshPending.insert(col);
        MeshJob* rawJob = job.get();
        m_streaming.meshJobs.push_back(std::move(job));
        // The worker meshes the job's OWN snapshot (no live World touch —
        // the generation-job pattern). m_blocks / m_atlas are immutable
        // after init (the render pass reads them concurrently). The chunk
        // mesh is a pure function of the snapshot, so the result is
        // deterministic (frame-locked FIFO integration).
        m_streaming.pool->post([this, rawJob] {
            const ChunkMesher mesher(rawJob->snap, m_blocks,
                                     m_atlas.layout());
            for (std::size_t cy = 0; cy < rawJob->meshes.size(); ++cy) {
                rawJob->meshes[cy] = mesher.mesh({rawJob->column.x,
                                                  static_cast<int>(cy),
                                                  rawJob->column.z});
            }
            rawJob->done = true;
        });
    }
}

} // namespace mc
