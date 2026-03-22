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
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // 添加任务到线程池
    template <class F>
    void enqueue(F &&f);

    // 获取当前任务数量
    size_t task_count() const;

private:
    // 工作线程
    std::vector<std::thread> workers;
    // 任务队列
    std::queue<std::function<void()>> tasks;

    // 同步机制
    mutable std::mutex queue_mutex;
    // 条件变量用于通知工作线程有新任务
    std::condition_variable condition;
    // 线程池停止标志
    bool stop;
    int max_queue_size = 1000; // 可选：限制任务队列的最大大小
};

template <class F>
void ThreadPool::enqueue(F &&f)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop)
        {
            // 如果线程池已经停止，抛出异常--throw
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        // 可选：限制任务队列的最大大小，防止内存过度使用
        if (works.size() > max_queue_size)
        {
            // 可以选择丢弃超出限制的任务(最旧任务)，或者抛出异常，或者记录日志,或者阻塞等待等
            throw std::runtime_error("Task queue overflow: too many tasks enqueued");
        }
        // forward--保证原有参数的类型和引用属性不变，完美转发
        tasks.emplace(std::forward<F>(f));
    }
    // 通知一个工作线程有新任务
    condition.notify_one();
}

#endif // THREAD_POOL_H
