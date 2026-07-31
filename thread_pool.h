#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// A fixed-size thread pool. Instead of spawning a new std::thread per
// incoming connection (expensive, and unbounded under load), we spawn
// a fixed number of worker threads up front and hand them jobs through
// a shared queue. This is the standard pattern used by real network
// servers to bound resource usage under high concurrency.
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) : stop_(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    void enqueue(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            jobs_.push(std::move(job));
        }
        cv_.notify_one();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;

    void workerLoop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty()) return;
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            job(); // run outside the lock so jobs don't block each other
        }
    }
};

#endif // THREAD_POOL_H
