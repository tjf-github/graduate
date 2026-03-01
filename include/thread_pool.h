#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <stdexcept>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();
    
    // 添加任务到线程池
    template<class F>
    void enqueue(F&& f);
    
    // 获取当前任务数量
    size_t task_count() const;
    
private:
    // 工作线程
    std::vector<std::thread> workers;
    // 任务队列
    std::queue<std::function<void()>> tasks;
    
    // 同步机制
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

template<class F>
void ThreadPool::enqueue(F&& f) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if(stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
}

#endif // THREAD_POOL_H
