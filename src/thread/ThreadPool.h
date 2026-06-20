#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t thread_count = 5);
    ~ThreadPool();

    bool start();
    void stop();

    // 把任务丢进线程池的任务队列
    // F: 函数类型（比如一个lambda或函数指针）
    // Args: 函数的参数
    template<typename F, typename... Args>
    void enqueue(F&& f, Args&&... args) {
        // 线程池已经关了？不收新任务
        if (!running_.load()) {
            return;
        }

        // bind: 把"函数"和"函数参数"打包成一个任务包
        // forward: 保持参数的原始类型（左值还是右值）不变
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // 上锁：防止读队列时别的线程正在往里加任务
        {
            // lock_guard: 上锁，防止多个线程同时往任务队列里塞任务
            std::lock_guard<std::mutex> lock(mutex_);
            // emplace: 把任务包放进队列尾部等待处理
            // move: 直接把任务移进去，不复制（更快）
            tasks_.emplace(std::move(task));
        }   // 出花括号时自动解锁

        // 唤醒一个正在睡觉的工作线程："有新任务了，快来取！"
        condition_.notify_one();
    }

    // 返回当前有多少个工作线程
    size_t threadCount() const { return workers_.size(); }

    // 返回任务队列中还有多少任务没处理
    size_t taskCount() const {
        // 上锁：防止读队列时别的线程正在往里加任务
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }   // 出花括号自动解锁

private:
    void worker();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> running_;
};

#endif
