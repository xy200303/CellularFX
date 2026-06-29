#ifndef CA_THREAD_POOL_H
#define CA_THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace ca {

// Simple fixed-size thread pool for recurring batch work. Threads are created
// once and reused across frames, avoiding the cost of spawning/joining threads
// for every parallel pass.
class ThreadPool {
public:
    explicit ThreadPool(size_t p_thread_count);
    ~ThreadPool();

    // Add a unit of work. Safe to call from any thread, but this simulator
    // submits entirely from the update thread.
    void enqueue(std::function<void()> p_task);

    // Block until the queue is empty and all workers have finished their
    // current task.
    void wait_all();

    size_t size() const { return workers.size(); }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable finished;

    bool stop = false;
    size_t active = 0;
};

} // namespace ca

#endif // CA_THREAD_POOL_H
