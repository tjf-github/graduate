#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads, int max) : stop(false), max_queue_size(max)
{
    for (size_t i = 0; i < num_threads; ++i)
    {
        workers.emplace_back([this]
                             {
            while(true) {
                // 从任务队列中取出一个任务，如果队列为空则等待，直到有新任务或线程池停止
                std::function<void()> task;
                //退出局部域，释放锁
                {
                    //智能锁定队列，确保线程安全
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] { 
                        return this->stop || !this->tasks.empty(); 
                    });
                    
                    if(this->stop && this->tasks.empty()) {
                        return;
                    }

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                
                // 执行任务，捕获并处理可能的异常，防止线程崩溃
                try {
                    task();
                } catch(const std::exception& e) {
                    std::cerr << "Task execution error: " << e.what() << std::endl;
                }
            } });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    // 通知所有工作线程停止等待并退出
    condition.notify_all();
    for (std::thread &worker : workers)
    {
        if (worker.joinable())
        {
            // 等待工作线程完成当前任务并退出，确保资源正确释放
            worker.join();
        }
    }
}

size_t ThreadPool::task_count() const
{
    // 获取当前任务数量，锁定队列以确保线程安全
    std::unique_lock<std::mutex> lock(queue_mutex);
    return tasks.size();
}
