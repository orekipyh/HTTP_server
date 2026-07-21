#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
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

    // ============================================================
    // submit — 把任务丢进线程池执行，执行完后能拿到返回值
    // ============================================================
    //
    // 这个函数是干什么的：
    //   把函数 f 和它需要的参数 args 打包成一份"工作"，交给线程池去跑。
    //   和 enqueue 不同的是，submit 会给你一张"取货单"（future），
    //   等线程跑完后，你可以用这张取货单拿到函数的返回值。
    //
    // 为什么这里要用模板？
    //   因为来的函数 f 可能是各种各样的——可能是普通函数、可能是 lambda、
    //   可能带 0 个参数也可能带 3 个参数。
    //   用模板就不用为每种情况单独写一个函数重载，让编译器自动适配。
    //   比如：
    //     submit(func1)         // f 不带参数
    //     submit(func2, a, b)  // f 带 2 个参数
    //     编译器会自动生成两个版本的 submit 代码
    //
    // 参数说明：
    //   f    — 要执行的函数（普通函数、lambda、或者任何能"调用"的东西）
    //   args — 传给 f 的参数，可以没有，也可以有多个
    //
    // 里面用到的变量说明：
    //   ReturnType — 编译器算出来的"f 返回什么类型"，比如 f 返回 int，那它就是 int
    //   task       — 一个"带返回值的函数包"，执行后会把返回值存起来等人来取
    //   result     — 用来取返回值的"取货单"，线程执行完后从它这里拿结果
    //
    // 返回值说明：
    //   返回一个"取货单"（future），调用方通过 .get() 就能拿到函数的返回值。
    //   如果任务还没跑完，.get() 会一直等，直到任务结束拿到结果。
    //
    // 什么时候会出错：
    //   如果线程池已经调用了 stop() 停止运行了，再 submit 就会报错。
    //
    // ============================================================
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        // 让编译器提前算一下：f(args...) 这个调用会返回什么类型？
        // 注意这只是让编译器算类型，不会真的去执行 f
        using ReturnType = decltype(f(args...));

        // make_shared<...>(...): 在内存里造一个东西，并返回它的"共享指针"
        // 这里造的是 packaged_task<ReturnType()>，可以把它理解成一个
        // "带返回值的函数包"——你调用它，它把返回值存起来，等人来取。
        //
        // 为什么要用共享指针包一层？
        //   因为这个"带返回值的函数包"不能复制，但后面要把它放进 lambda 表达式里，
        //   lambda 要求抓进来的东西能复制。所以用共享指针在外面包一层，
        //   共享指针可以复制，里面的函数包还是同一个。
        //
        // bind(f, args...):
        //   把函数 f 和它的参数 args 打包成一个"不需要参数就能调用的东西"。
        //   比如 bind(add, 1, 2) 打包后，你直接调用它就等于 add(1, 2)
        //
        // forward<F>(f):
        //   保持 f 本来的"左值/右值"性质不变。简单说就是：
        //   如果你传进来的是临时对象，就把它"移动"进去（省一次拷贝）；
        //   如果你传进来的是普通变量，就正常复制。
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        // 从"带返回值的函数包"里取出"取货单"。
        // 将来工作线程执行了这个函数包，返回值存好后，
        // 调用方就可以通过这张取货单（result.get()）把结果取出来。
        std::future<ReturnType> result = task->get_future();

        {
            // lock_guard: 上一把锁，防止多个线程同时操作任务队列
            std::lock_guard<std::mutex> lock(mutex_);

            // 线程池已经关了？不收新任务
            if (!running_.load()) {
                throw std::runtime_error("ThreadPool stopped, cannot submit new task");
            }

            // 把 lambda 放进任务队列，等着工作线程来取。
            // 这个 lambda 抓取了 task（共享指针），所以它里面存的是指向"函数包"的指针。
            // 执行时：(*task)() 就是调用那个"带返回值的函数包"，
            // 函数执行完，返回值自动存好，取货单那边就能拿了。
            // emplace: 直接在队列尾巴上造出这个 lambda，省一次拷贝。
            tasks_.emplace([task]() { (*task)(); });
        }

        // 叫醒一个正在睡觉的工作线程："有新活了，起来干活！"
        condition_.notify_one();

        // 把取货单返回给调用方，后面通过 .get() 拿结果
        return result;
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
