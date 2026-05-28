#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace core
{

class ThreadPool
{
public:
    ThreadPool() = default;
    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    void start(std::size_t threadCount);
    void stop();

    void enqueue(std::function<void()> job);

private:
    void workerLoop();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::function<void()>> m_jobs;
    std::vector<std::thread> m_threads;

    bool m_running = false;
};

} // namespace core

