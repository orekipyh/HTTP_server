#include <cstring>
#include <cerrno>
#include <iostream>
#include <arpa/inet.h>

#include "HttpServer.h"
#include "Socket.h"
#include "Epoll.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "thread/ThreadPool.h"
#include "../utils/FileUtil.h"

// 构造函数：初始化服务器配置
HttpServer::HttpServer(int port, const std::string& root_dir)
    : port_(port), root_dir_(root_dir), running_(false) {
}

// 析构函数：停止服务器并清理资源
HttpServer::~HttpServer() {
    stop();
}

// 启动HTTP服务器
bool HttpServer::start() {
    // 初始化服务器socket
    if (!initSocket()) {
        std::cerr << "Failed to initialize socket" << std::endl;
        return false;
    }

    // 初始化epoll
    if (!initEpoll()) {
        std::cerr << "Failed to initialize epoll" << std::endl;
        return false;
    }

    running_ = true;
    std::cout << "HTTP Server listening on port " << port_ << std::endl;
    //static files指的是静态文件，如HTML、CSS、JavaScript等，不包含动态内容
    //该项目中www文件夹下的文件就是静态文件
    std::cout << "Serving static files from: " << root_dir_ << std::endl;

    // 创建并启动线程池（5个工作线程）
    // 这里线程池是被共享指针管理的，
    // 之所以用unique_ptr管理，是让线程池在使用时才创建，避免了资源浪费
    thread_pool_ = std::make_unique<ThreadPool>(5);
    thread_pool_->start();

    // 启动事件循环
    // 使用lambda表达式作为回调函数处理事件
    epoll_->setCallback([this](int fd, uint32_t events) {
        // 如果是服务器socket可读，表示有新连接到来
        if (fd == server_socket_->getFd()) {
            handleNewConnection();
        } else {
            // 客户端socket事件
            // |是或操作，EPOLLIN EPOLLUP EPOLLERR都是二进制
            // 把它们或上作为掩码和events做与操作，就可以判断events是否包含这三种事件
            if (events & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
                handleClientRequest(fd);
            }
        }
    });

    // 开始事件循环，-1表示阻塞等待
    epoll_->start(-1);

    return true;
}

// 停止HTTP服务器
void HttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& conn : connections_) {
            if (conn.fd >= 0) {
                ::close(conn.fd);
                conn.fd = -1;
            }
        }
        connections_.clear();
    }

    // 停止epoll事件循环
    if (epoll_) {
        epoll_->stop();
    }

    // 停止线程池，等待所有任务完成
    if (thread_pool_) {
        thread_pool_->stop();
    }

    std::cout << "HTTP Server stopped" << std::endl;
}

// 初始化服务器socket
bool HttpServer::initSocket() {
    // 创建Socket对象
    server_socket_ = std::make_unique<Socket>();

    // 创建TCP socket
    if (!server_socket_->create()) {
        std::cerr << "Failed to create socket: " << std::strerror(errno) << std::endl;
        return false;
    }

    // 设置SO_REUSEADDR，允许端口复用
    if (!server_socket_->setReuseAddr(true)) {
        std::cerr << "Failed to set SO_REUSEADDR: " << std::strerror(errno) << std::endl;
        return false;
    }

    // 绑定端口
    if (!server_socket_->bind(port_)) {
        std::cerr << "Failed to bind to port " << port_ << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    // 开始监听连接
    if (!server_socket_->listen(1024)) {
        std::cerr << "Failed to listen on port " << port_ << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    // 设置socket为非阻塞模式
    if (!server_socket_->setNonBlocking(true)) {
        std::cerr << "Failed to set non-blocking mode" << std::endl;
        return false;
    }

    return true;
}

// 初始化epoll
bool HttpServer::initEpoll() {
    // 创建Epoll对象
    epoll_ = std::make_unique<Epoll>();

    // 创建epoll实例
    if (!epoll_->create(1024)) {
        std::cerr << "Failed to create epoll" << std::endl;
        return false;
    }

    // 将服务器socket注册到epoll
    // 使用水平触发(LT)模式（默认），避免边缘触发(ET)下并发连接漏接的问题
    // 当accept队列中还有连接时，LT模式会持续触发EPOLLIN事件
    if (!epoll_->add(server_socket_->getFd(), EPOLLIN)) {
        std::cerr << "Failed to add server socket to epoll" << std::endl;
        return false;
    }

    return true;
}

// 处理新连接
// 用 while 循环一口气把所有待处理的连接都 accept 掉。
// 假设一瞬间来了 10 个客户端，epoll 会通知"有新连接"，
// 如果只 accept 一次就退出，那就只接进来 1 个，剩下 9 个还要等 epoll 再次通知。
// 用 while 循环可以一次全部接完，效率更高。
// （如果是边缘触发 ET 模式，不用 while 甚至会漏接连接。）
void HttpServer::handleNewConnection() {
    while (true) {
        struct sockaddr_in client_addr;              // 存客户端信息（IP地址+端口号）
        memset(&client_addr, 0, sizeof(client_addr));

        // 循环接受所有待处理的连接
        int client_fd = server_socket_->accept(&client_addr);  // client_fd: 新客户端的"身份证号", -1 表示没接到
        if (client_fd < 0) {
            // EAGAIN 或 EWOULDBLOCK 表示所有连接已处理完毕
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;                               // 没有更多新连接了，退出 while 循环
            }
            std::cerr << "Failed to accept connection: " << std::strerror(errno) << std::endl;
            break;
        }

        // 设置客户端socket为非阻塞模式
        int flags = ::fcntl(client_fd, F_GETFL, 0);  // flags: 当前 socket 的"开关状态"
        if (flags == -1) {
            std::cerr << "Failed to get socket flags: " << std::strerror(errno) << std::endl;
            ::close(client_fd);
            continue;                                // 跳过这个客户端，继续接受下一个
        }
        flags |= O_NONBLOCK;                         // 追加"非阻塞"这个开关
        if (::fcntl(client_fd, F_SETFL, flags) == -1) {
            std::cerr << "Failed to set non-blocking: " << std::strerror(errno) << std::endl;
            ::close(client_fd);
            continue;
        }

        // 将客户端socket注册到epoll（使用水平触发LT模式）
        if (!epoll_->add(client_fd, EPOLLIN | EPOLLRDHUP)) {
            std::cerr << "Failed to add client socket to epoll" << std::endl;
            ::close(client_fd);
            continue;
        }

        // 添加到连接列表
        Connection conn;                             // conn: 记录这个客户端的"档案卡"
        conn.fd = client_fd;                         // 档案卡上记下身份证号
        conn.buffer_len = 0;                         // 当前还没收到数据，缓冲区长度 0
        conn.keep_alive = false;                     // 暂不支持长连接
        {
            std::lock_guard<std::mutex> lock(mutex_); // lock: 上锁，防止多个线程同时修改列表
            connections_.push_back(conn);             // 把档案卡放进连接列表中
        }                                             // 出了大括号，lock 自动解锁

        std::cout << "New connection from: "
                  << inet_ntoa(client_addr.sin_addr) << ":"
                  << ntohs(client_addr.sin_port) << std::endl;
    }
}

// 处理客户端请求
// 主线程中仅负责接收数据，然后将请求处理提交到线程池
void HttpServer::handleClientRequest(int client_fd) {
    // 读取客户端数据（水平触发模式下，单次读取即可）
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            std::cerr << "Error reading from client: " << std::strerror(errno) << std::endl;
        }
        closeConnection(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';
    std::string request_data(buffer, bytes_read);

    std::cout << "Received request from fd " << client_fd << std::endl;

    // 将请求处理提交到线程池
    // 工作线程将执行：解析请求、读取文件、构建响应、发送响应、关闭连接
    thread_pool_->enqueue([this, client_fd, request_data]() {
        processRequest(client_fd, request_data);
    });
}

// 在线程池中处理请求（解析、文件读取、响应构建、发送、关闭连接）
void HttpServer::processRequest(int client_fd, const std::string& request_data) {
    // 解析HTTP请求
    HttpRequest http_request;
    //parse是解析函数，返回布尔值，解析的结果存在HttpRequest对象中
    if (!http_request.parse(request_data)) {
        handleError(client_fd, 400);
        return;
    }

    // 规范化请求路径（安全检查）
    std::string file_path = http_request.normalizePath();

    // 构建实际文件路径
    std::string full_path = root_dir_;
    if (!file_path.empty() && file_path[0] == '/') {
        //如果file_path开头有/，就去掉这个/，因为root_dir_已经包含了/了
        full_path += file_path.substr(1);
    } else {
        full_path += file_path;
    }

    // 使用FileUtil读取文件
    FileUtil file_util(full_path);
    if (!file_util.readFile()) {
        std::cerr << "File not found: " << full_path << std::endl;
        handleError(client_fd, 404);
        return;
    }

    // 生成HTTP响应
    HttpResponse response;
    // 设置响应状态码为200（成功）
    response.setStatusCode(200);
    // 设置响应内容类型（根据文件路径）
    response.setContentTypeByPath(file_path);
    // 设置响应体（文件内容）
    response.setBody(file_util.content());

    // 发送响应
    sendResponse(client_fd, response);

    // 响应完成后关闭连接（Connection: close）
    closeConnection(client_fd);
}

// 发送响应给客户端
void HttpServer::sendResponse(int client_fd, const HttpResponse& response) {
    std::string resp_str = response.toString();
    ssize_t bytes_sent = send(client_fd, resp_str.c_str(), resp_str.size(), 0);

    if (bytes_sent < 0) {
        std::cerr << "Error sending response to client" << std::endl;
    }
}

// 关闭客户端连接
void HttpServer::closeConnection(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 从epoll中移除
        if (epoll_) {
            epoll_->remove(client_fd);
        }

        // 从连接列表中移除
        for (auto it = connections_.begin(); it != connections_.end(); ++it) {
            if (it->fd == client_fd) {
                connections_.erase(it);
                break;
            }
        }
    }

    // 关闭socket（不在锁内执行，避免不必要的持锁时间）
    ::close(client_fd);

    std::cout << "Connection closed: fd " << client_fd << std::endl;
}

// 错误处理：生成错误响应
void HttpServer::handleError(int client_fd, int status_code) {
    HttpResponse response;

    if (status_code == 404) {
        response = HttpResponse::notFound();
    } else if (status_code == 500) {
        response = HttpResponse::internalError();
    } else {
        response.setStatusCode(status_code);
        response.setContentType("text/html");
        response.setBody("<html><body><h1>Bad Request</h1></body></html>");
    }

    sendResponse(client_fd, response);

    // 错误响应完成后也关闭连接
    closeConnection(client_fd);
}
