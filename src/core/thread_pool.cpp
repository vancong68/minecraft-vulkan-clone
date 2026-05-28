#include "thread_pool.hpp"

namespace core
{

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::start(std::size_t threadCount)
{
    stop();

    if (threadCount == 0) {
        threadCount = 1;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = true;
    }

    m_threads.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        m_threads.emplace_back([this] { workerLoop(); });
    }
}

void ThreadPool::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    m_cv.notify_all();

    for (auto &t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_threads.clear();

    std::queue<std::function<void()>> empty;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::swap(m_jobs, empty);
    }
}

void ThreadPool::enqueue(std::function<void()> job)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) {
            return;
        }
        m_jobs.push(std::move(job));
    }
    m_cv.notify_one();
}

void ThreadPool::workerLoop()
{
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] { return !m_running || !m_jobs.empty(); });
            if (!m_running && m_jobs.empty()) {
                return;
            }
            job = std::move(m_jobs.front());
            m_jobs.pop();
        }

        if (job) {
            job();
        }
    }
}

} // namespace core

