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

class ThreadPool
{
public:
    // max_queue_size: 队列上限；超过后 enqueue 抛异常
    explicit ThreadPool(size_t num_threads, int max_queue_size = 100);
    ~ThreadPool();

    // 添加任务到线程池
    template <class F>
    void enqueue(F &&f);

    // 获取当前任务数量
    size_t task_count() const;

private:
    // 工作线程集合
    std::vector<std::thread> workers;
    // 任务队列
    std::queue<std::function<void()>> tasks;

    // 同步机制
    mutable std::mutex queue_mutex;
    // 通知工作线程有新任务或停止信号
    std::condition_variable condition;
    // 停止标记
    bool stop;

    // 最大任务队列容量
    int max_queue_size;
};

template <class F>
void ThreadPool::enqueue(F &&f)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        // 队列满时快速失败，避免无限堆积任务
        if (tasks.size() >= (size_t)max_queue_size)
        {
            // 如果队列已满，这里可以采取丢弃策略、抛出异常或阻塞
            // 简单起见，这里抛出异常
            throw std::runtime_error("ThreadPool queue is full");
        }
        // 将任务添加到队列中
        tasks.emplace(std::forward<F>(f));
    }
    // 通知一个工作线程有新任务可执行
    condition.notify_one();
}

#endif // THREAD_POOL_H
