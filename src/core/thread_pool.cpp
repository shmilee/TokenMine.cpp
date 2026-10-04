#include "core/thread_pool.hpp"

namespace mc {

ThreadPool::ThreadPool(int workers) {
    if (workers < 1) {
        workers = 1;
    }
    m_workers.reserve(static_cast<std::size_t>(workers));
    for (int i = 0; i < workers; ++i) {
        m_workers.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopped = true;
    }
    m_cv.notify_all();
    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::post(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(job));
        ++m_inFlight;
    }
    m_cv.notify_one();
}

void ThreadPool::flush() {
    std::unique_lock<std::mutex> lock(m_mutex);
    // Jobs never post further jobs (game jobs are leaf work), so once the
    // in-flight count reaches zero nothing more can be posted from a
    // worker; the main thread is the only other poster and it is blocked
    // here.
    m_cv.wait(lock, [this] { return m_inFlight == 0; });
    if (m_jobException != nullptr) {
        std::exception_ptr e = std::move(m_jobException);
        m_jobException = nullptr;
        lock.unlock();
        std::rethrow_exception(e);
    }
}

void ThreadPool::workerLoop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
        while (m_queue.empty()) {
            if (m_stopped) {
                return;
            }
            m_cv.wait(lock);
        }
        // Drain whatever is available; re-acquire idle state between jobs
        // so a newly posted job can wake another worker.
        auto job = std::move(m_queue.front());
        m_queue.pop_front();
        lock.unlock();
        try {
            job();
        } catch (...) {
            std::lock_guard<std::mutex> guard(m_mutex);
            if (m_jobException == nullptr) {
                m_jobException = std::current_exception();
            }
        }
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            --m_inFlight;
        }
        m_cv.notify_all();
        lock.lock();
    }
}

} // namespace mc
