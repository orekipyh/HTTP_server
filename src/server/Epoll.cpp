#include "Epoll.h"
#include <unistd.h>
#include <cerrno>

// 构造函数：初始化epoll_fd_为-1，running_为false
Epoll::Epoll() : epoll_fd_(-1), running_(false) {}

// 析构函数：关闭epoll文件描述符
Epoll::~Epoll() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

// 创建epoll实例
// epoll_create: 创建一个epoll文件描述符
// size: 监听的最大fd数量（Linux 2.6.8以后被忽略，但必须大于0）
bool Epoll::create(int size) {
    epoll_fd_ = ::epoll_create(size);
    return epoll_fd_ >= 0;
}

// 注册事件到epoll
// 注册就是把文件描述符放入内核的红黑树中
// EPOLL_CTL_ADD: 将fd注册到epoll实例
// ev.events: 要监听的事件类型
// ev.data.fd: 关联的用户数据（通常是被监听的fd）
bool Epoll::add(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    //::epoll_ctl是系统调用
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

// 修改已注册的事件
// EPOLL_CTL_MOD: 修改已注册fd的事件
bool Epoll::modify(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

// 从epoll中删除事件
// EPOLL_CTL_DEL: 从epoll实例中删除fd
bool Epoll::remove(int fd) {
    return ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

// 等待事件触发
// epoll_wait: 阻塞等待注册的事件发生
// maxevents: 最多返回的事件数量
// timeout: 超时时间（毫秒），-1表示无限等待
// 返回值: 触发的事件数量，0表示超时，-1表示出错
int Epoll::wait(std::vector<epoll_event>& events, int timeout) {
    // 这里的events数组就是内核态的就绪事件数组
    // 预分配events容量避免频繁内存分配
    if (events.empty()) {
        events.resize(64);
    }

    // 调用epoll_wait等待事件，当有事件触发时，返回触发的事件数量，否则返回0
    // events.data(): 返回的事件数组指针
    // events.size(): 数组大小
    // epoll_fd_: epoll实例的文件描述符
    // timeout: 超时时间（毫秒），-1表示阻塞等待，不会在等待一段时间后返回结果
    // 而是在有事件触发时返回触发的事件数量
    int nfds = ::epoll_wait(epoll_fd_, events.data(), events.size(), timeout);
    return nfds;
}

// 设置事件回调函数
// callback: 事件触发时调用的回调函数
// fd: 触发事件的描述符
// events: 触发的事件类型
void Epoll::setCallback(const EventCallback& callback) {
    callback_ = callback;
}

// 启动事件循环
// timeout: epoll_wait的超时时间（毫秒），-1表示阻塞等待
// 循环流程：wait() -> processEvents() -> 调用callback_
void Epoll::start(int timeout) {
    running_ = true;
    std::vector<epoll_event> events;

    // 事件循环：持续等待并处理事件，直到stop()被调用
    while (running_) {
        // 调用wait()阻塞等待事件触发
        int nfds = wait(events, timeout);

        // epoll_wait 可能被信号中断返回 -1（errno == EINTR）
        // 这种情况下应该继续等待，而不是退出事件循环
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        // 处理触发的事件
        if (nfds > 0) {
            processEvents(events, nfds);
        }

        //因为timeout=-1，所以nfds永远不会=0，所以if(nfds==0)没必要写
    }

    // 当while异常退出时，running_会被设置为false，
    // 防止其他线程继续调用start()中的循环
    running_ = false;
}

// 停止事件循环
// 通过设置running_为false来通知start()中的循环退出
void Epoll::stop() {
    running_ = false;
}

// 处理已触发的事件
// events: wait()返回的事件数组
// nfds: 实际触发的事件数量
// 遍历所有事件，根据事件类型调用回调函数处理
void Epoll::processEvents(const std::vector<epoll_event>& events, int nfds) {
    // 如果没有设置回调函数，直接返回
    if (!callback_) {
        return;
    }

    // 遍历所有触发的事件
    for (int i = 0; i < nfds; ++i) {
        int fd = events[i].data.fd;
        uint32_t event_type = events[i].events;

        // 调用用户设置的回调函数处理事件
        // 回调函数根据event_type（EPOLLIN/EPOLLOUT/EPOLLERR等）做相应处理
        callback_(fd, event_type);
    }
}
