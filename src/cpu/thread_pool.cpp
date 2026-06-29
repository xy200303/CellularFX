#include "thread_pool.h"

namespace ca {

ThreadPool::ThreadPool(size_t p_thread_count) {
    workers.reserve(p_thread_count);
    for (size_t i = 0; i < p_thread_count; i++) {
        workers.emplace_back([this]() {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this]() { return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) {
                        return;
                    }
                    task = std::move(tasks.front());
                    tasks.pop();
                    active++;
                }
                task();
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    active--;
                    if (active == 0 && tasks.empty()) {
                        finished.notify_all();
                    }
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::enqueue(std::function<void()> p_task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.push(std::move(p_task));
    }
    condition.notify_one();
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    finished.wait(lock, [this]() { return tasks.empty() && active == 0; });
}

} // namespace ca
