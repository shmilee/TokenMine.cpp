// Thread pool (Phase 14): a fixed set of worker threads running posted
// jobs, with a per-frame synchronization point (flush).
//
// The game's pipeline contract (what makes chunk streaming race-free):
//   flush()                  <- every job posted before this call is
//                                complete; the main thread now owns all
//                                writes...
//   ...main-thread world writes (integrate, interaction, fluids)...
//   ...mesh + render (main thread only READS the world)...
//   post(gen/light jobs)     <- workers run from here on; the main thread
//                                stops writing the world until the next
//   (next frame) flush()     <- the workers' window ends before the next
//                                write window opens.
//
// So a job touches shared state only while the main thread is in its
// read-only window. Generation jobs touch no shared mutable state at all
// (WorldGenerator::generateChunk is a pure function); lighting jobs write
// only light arrays and the dirty flag, which the main thread does not
// write concurrently (meshing bakes light into vertices but never writes
// the arrays).
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
