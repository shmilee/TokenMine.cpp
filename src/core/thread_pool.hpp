// Thread pool (Phase 14): a fixed set of worker threads running posted
// jobs, with a per-frame synchronization point (flush).
//
// The game's pipeline contract (what makes chunk streaming race-free):
//   flush()                  <- every job posted before this call is
//                                complete; the main thread now owns all
//                                writes...
//   ...main-thread world writes (integrate, interaction, fluids)...
//   ...GL rebuild + render (main thread only READS the world)...
//   post(gen -> mesh -> light) <- workers run from here on; the main
//                                thread stops writing the world until the
//                                next flush (99.10: up to TWO lighting
//                                jobs, parallel only for bit-level-
//                                independent regions — planLightJobs)
//   (next frame) flush()     <- the workers' window ends before the next
//                                write window opens.
//
// So a job touches shared state only while the main thread is in its
// read-only window. Generation jobs touch no shared mutable state at all
// (WorldGenerator::generateChunk is a pure function); MESH jobs (PIPE-1)
// also touch no shared mutable state — the mesher runs on the job's own
// snapshot World, copied at post time (after every main-thread World
// write of the frame, BEFORE the lighting post, which is why the post
// order is gen -> mesh -> light); lighting jobs write only light arrays
// of DISJOINT chunks (99.10: at most two per frame, parallel only when
// their regions share no chunk — write footprint AND the block-light
// boundary-read ring) and collect their net-changed chunk positions
// job-locally — the World's shared dirty set is main-thread-only
// (applied at collectLitColumns after the flush), which the main thread
// does not write concurrently (the next frame's mesh snapshot is taken
// after the light lands).
//
// Flush budget (kStreamWorkerCount = 4, FIFO): a fill frame posts 2 gen
// (~1.6 ms) + 2 mesh (<= 6 ms) + up to 2 streaming light (one box each,
// ~8.4 ms) + up to 2 fluid catch-up light (99.11, one box each, ~8.4 ms
// — posted only when every fluid candidate is lightRegionsIndependent
// of every streaming column, so all four light jobs' regions are
// pairwise chunk-disjoint and parallel-safe): 8 jobs > 4 workers — the
// gen jobs take two workers (0..1.6), the mesh jobs the other two
// (0..6), and the four light jobs (posted last) run on the freed
// workers in two waves (~1.6..10 and ~6..14.4) — flush ≈ 14.4 ms when
// a fill frame coincides with active coast churn (the 99.10 floor
// ~10 ms holds in streaming-quiet frames; pre-99.10 was 13 + 1.6 with
// the single sequential 2-box job). A queued job only adds latency,
// never a wrong result: flush() waits for EVERY posted job.
//
// Exception safety: game jobs cannot fail (the generator and lighting are
// infallible). If a job did throw, the first exception is rethrown from
// the next flush() or the destructor, converting a worker-thread failure
// into a controlled main-thread failure.
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace mc {

class ThreadPool {
public:
    // Spawns `workers` threads (at least 1). The pool is idle until
    // post(); the destructor stops the workers and joins them, running
    // any pending jobs to completion first.
    explicit ThreadPool(int workers);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Queue a job for a worker. Thread-safe. Returns immediately; use
    // flush() to wait for completion.
    void post(std::function<void()> job);

    // Block until every job posted so far (from any thread) has completed.
    // Returns immediately when nothing is pending. Rethrows the first job
    // exception, if any (and clears it).
    void flush();

    std::size_t size() const { return m_workers.size(); }

    // Jobs posted but not yet finished (queued + running). The streaming
    // loop bounds the backlog with this so the per-frame flush() — which
    // waits for EVERY in-flight job — stays inside the frame budget.
    std::size_t pendingCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<std::size_t>(m_inFlight);
    }

private:
    void workerLoop();

    std::vector<std::thread> m_workers;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_queue;
    int m_inFlight = 0; // posted but not yet finished
    bool m_stopped = false;
    std::exception_ptr m_jobException = nullptr;
};

} // namespace mc
