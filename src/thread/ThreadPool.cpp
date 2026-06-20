#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t thread_count)
    : running_(false) {
    workers_.reserve(thread_count);
}

ThreadPool::~ThreadPool() {
    stop();
}

bool ThreadPool::start() {
    if (running_.load()) {
        return false;
    }

    running_ = true;

    size_t thread_count = workers_.capacity();
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::worker, this);
    }

    std::cout << "ThreadPool started with " << thread_count << " threads" << std::endl;
    return true;
}

void ThreadPool::stop() {
    if (!running_.load()) {
        return;
    }

    running_ = false;

    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!tasks_.empty()) {
            tasks_.pop();
        }
    }
}

// 工作线程的主循环
// 每个线程都在这个函数里转圈：等任务 → 干活 → 等下一个任务
void ThreadPool::worker() {
    // 只要线程池没关，就一直转
    while (running_.load()) {
        std::function<void()> task;

        {
            // unique_lock: 比 lock_guard 灵活，能配合 condition_variable 使用
            std::unique_lock<std::mutex> lock(mutex_);

            // wait: 解锁并睡觉，等别人叫醒
            // lambda 条件：有任务可做 或 线程池要关了
            // 如果条件不满足，线程继续睡(阻塞)
            // 如果条件满足，线程醒来并自动重新上锁(解除阻塞)
            // 条件满足：有任务可做 或 线程池要关了
            condition_.wait(lock, [this]() {
                return !tasks_.empty() || !running_.load();
            });

            // 如果线程池要关了，而且也没有积压的任务了 → 退出
            if (!running_.load() && tasks_.empty()) {
                return;
            }

            // 从队列头部取出一个任务
            task = std::move(tasks_.front());  // move 避免复制，效率更高
            tasks_.pop();                       // 把这个任务从队列里删掉
        }   // 出花括号自动解锁，让别的线程能拿任务

        // 执行任务
        try {
            task();  // 调用真正的函数（如 processRequest）
        // exception: 是一个类型，所有异常的基类型
        } catch (const std::exception& e) {
            // 捕获已知异常，防止一个任务崩溃导致整个线程挂掉
            std::cerr << "ThreadPool task exception: " << e.what() << std::endl;
        } catch (...) {
            // 捕获未知异常（如除以 0、空指针等）
            std::cerr << "ThreadPool task unknown exception" << std::endl;
        }

        // 执行完回到 while 循环顶部，等下一个任务
    }
}
