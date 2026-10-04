// ThreadPool tests (Phase 14): execution, flush semantics, worker-thread
// execution, exception propagation, destructor join, and a stress pass.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

#include "core/thread_pool.hpp"

using namespace mc;

TEST(ThreadPoolTest, JobsRunAndFlushWaits) {
    ThreadPool pool(2);
    EXPECT_EQ(pool.size(), 2u);
    std::atomic<int> done{0};
    for (int i = 0; i < 100; ++i) {
        pool.post([&done] { done.fetch_add(1); });
    }
    pool.flush();
    EXPECT_EQ(done.load(), 100);
    // A flush with nothing pending returns immediately.
    pool.flush();
    EXPECT_EQ(done.load(), 100);
}

TEST(ThreadPoolTest, JobsRunOnWorkerThreads) {
    ThreadPool pool(1);
    const std::thread::id mainId = std::this_thread::get_id();
    bool ranOnWorker = true;
    pool.post([&ranOnWorker, mainId] {
        ranOnWorker = std::this_thread::get_id() != mainId;
    });
    pool.flush();
    EXPECT_TRUE(ranOnWorker);
}

TEST(ThreadPoolTest, FlushBlocksUntilSlowJobCompletes) {
    ThreadPool pool(2);
    bool ready = false;
    pool.post([&ready] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        ready = true;
    });
    // The flush must not return before the job has run to completion.
    pool.flush();
    EXPECT_TRUE(ready);
}

TEST(ThreadPoolTest, JobExceptionRethrownAtFlush) {
    ThreadPool pool(2);
    pool.post([] { throw std::runtime_error("job failed"); });
    EXPECT_THROW(pool.flush(), std::runtime_error);
    // The pool is still fully usable after a job failure.
    bool ran = false;
    pool.post([&ran] { ran = true; });
    pool.flush();
    EXPECT_TRUE(ran);
}

TEST(ThreadPoolTest, FirstExceptionWinsOnSingleWorker) {
    // One worker: the jobs run in posted (FIFO) order, so the first
    // exception is deterministic.
    ThreadPool pool(1);
    pool.post([] { throw std::runtime_error("first"); });
    pool.post([] { throw std::logic_error("second"); });
    try {
        pool.flush();
        FAIL() << "expected the first job's exception to be rethrown";
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "first");
    }
    // The exception is cleared: a later failure reports its own.
    pool.post([] { throw std::logic_error("third"); });
    EXPECT_THROW(pool.flush(), std::logic_error);
}

TEST(ThreadPoolTest, DestructorRunsPendingJobs) {
    std::atomic<int> done{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 50; ++i) {
            pool.post([&done] { done.fetch_add(1); });
        }
        // No explicit flush: the destructor must run the pending jobs
        // before it joins the workers.
    }
    EXPECT_EQ(done.load(), 50);
}

TEST(ThreadPoolTest, InterleavedPostAndFlush) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    for (int round = 0; round < 20; ++round) {
        for (int i = 0; i < 25; ++i) {
            pool.post([&counter] { counter.fetch_add(1); });
        }
        pool.flush();
        EXPECT_EQ(counter.load(), (round + 1) * 25);
    }
}

TEST(ThreadPoolTest, StressManyJobs) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    for (int i = 0; i < 5000; ++i) {
        pool.post([&counter] { counter.fetch_add(1); });
    }
    pool.flush();
    EXPECT_EQ(counter.load(), 5000);
}
