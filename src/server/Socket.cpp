#include <cstring>
#include <cerrno>
#include <iostream>
#include "Socket.h"

// 构造函数：初始化socket描述符为-1
Socket::Socket() : fd_(-1) {}

// 析构函数：确保socket被关闭
Socket::~Socket() {
    close();
}

// 创建TCP socket
// AF_INET: IPv4协议
// SOCK_STREAM: TCP流式套接字(数据无边界传输)
// 0: 默认协议（TCP）
// 套接字=IP地址+端口号，相当于数据通道的端点
bool Socket::create() {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    return fd_ >= 0;
}

// 设置SO_REUSEADDR选项，允许端口复用
// 当服务器重启时，可以立即绑定到之前使用的端口
// on: 是否设置为端口复用模式
bool Socket::setReuseAddr(bool on) {
    int opt = on ? 1 : 0;
    // SOL_SOCKET: 在socket层(通用层)设置允许端口复用
    // SO_REUSEADDR: 允许端口复用
    // opt: 选项值，1表示开启，0表示关闭
    // sizeof(opt): 选项值的字节数
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

// 设置SO_KEEPALIVE选项，开启TCP保活机制
// 检测客户端连接是否存活，防止僵尸连接占用资源
bool Socket::setKeepAlive(bool on) {
    int opt = on ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == 0;
}

// 设置socket为非阻塞模式
// O_NONBLOCK: 非阻塞I/O模式
// fcntl: 文件描述符控制函数
// F_GETFL: 获取文件描述符的标志位
// F_SETFL: 设置文件描述符的标志位
// 0: 用于获取当前标志位值
bool Socket::setNonBlocking(bool on) {
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags == -1) return false;

    // on: 是否设置为非阻塞模式
    // 如果为true，设置为非阻塞模式
    // 如果为false，清除非阻塞模式
    if (on) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    return ::fcntl(fd_, F_SETFL, flags) == 0;
}

// 绑定端口到socket
// INADDR_ANY: 绑定到所有可用网络接口
// htons: 主机字节序转网络字节序（大端序）
bool Socket::bind(int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    return ::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) == 0;
}

// 监听socket上的连接
// backlog: 最大连接队列长度（等待accept处理的连接数）
bool Socket::listen(int backlog) {
    return ::listen(fd_, backlog) == 0;
}

// 接受客户端连接
// 返回新的客户端socket描述符
// addr: 可选参数，用于获取客户端地址信息
// fd_: 服务器socket描述符，
// 这个是在类里定义的，是Socket类的共享成员变量
// 这里的共享不是static那种，而是每个对象和函数成员都共享一个fd_变量
int Socket::accept(struct sockaddr_in* addr) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    return ::accept(fd_, (struct sockaddr*)addr, &addr_len);
}

// 关闭socket
// 先检查fd_是否有效，然后调用close关闭
void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}