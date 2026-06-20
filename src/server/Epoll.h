// Epoll.h - Epoll多路复用封装类头文件
#ifndef EPOLL_H
#define EPOLL_H

#include <sys/types.h>
#include <sys/epoll.h>
#include <vector>
#include <atomic>
#include <functional>

// Epoll事件处理回调函数类型
// fd: 触发事件的文件描述符
// events: 触发的事件类型（EPOLLIN/EPOLLOUT等）
using EventCallback = std::function<void(int fd, uint32_t events)>;

class Epoll {
public:
    // 构造函数
    Epoll();

    // 析构函数，关闭epoll文件描述符
    ~Epoll();

    // 创建epoll实例
    // size: epoll实例大小（建议为最大监听fd数量）
    bool create(int size = 1024);

    // 注册事件到epoll
    // fd: 要监听的文件描述符
    // events: 要监听的事件类型（EPOLLIN/EPOLLOUT/EPOLLERR等）
    bool add(int fd, uint32_t events);

    // 修改已注册的事件
    bool modify(int fd, uint32_t events);

    // 从epoll中删除事件
    bool remove(int fd);

    // 等待事件触发
    // events: 输出参数，存储触发的事件
    // timeout: 超时时间（毫秒），-1表示阻塞等待
    // 返回: 触发的事件数量
    int wait(std::vector<epoll_event>& events, int timeout = -1);

    // 设置事件回调
    void setCallback(const EventCallback& callback);

    // 获取epoll文件描述符
    int getFd() const { return epoll_fd_; }

    // 事件循环处理：启动事件循环
    // 内部调用wait()获取事件，然后逐个调用回调函数处理
    void start(int timeout = -1);

    // 事件循环处理：停止事件循环
    // 设置running_为false，下一次wait()返回后将退出循环
    void stop();

    // 处理已触发的事件
    // 遍历events中所有事件，根据事件类型调用回调函数
    // EPOLLIN: 读事件，表示有数据可读或新连接到来
    // EPOLLOUT: 写事件，表示可以写入数据
    // EPOLLERR: 错误事件
    // EPOLLHUP: 挂起事件，通常表示对端关闭连接
    void processEvents(const std::vector<epoll_event>& events, int nfds);

    // 检查事件循环是否正在运行
    bool isRunning() const { return running_.load(); }

private:
    int epoll_fd_;                      // epoll文件描述符
    EventCallback callback_;            // 事件回调函数
    std::atomic<bool> running_;        // 事件循环运行标志
};

#endif // EPOLL_H
