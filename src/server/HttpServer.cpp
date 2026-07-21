#include <cstring>
#include <cerrno>
#include <iostream>
#include <algorithm>
#include <vector>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "HttpServer.h"
#include "Socket.h"
#include "Epoll.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "ServerLogger.h"
#include "thread/ThreadPool.h"
#include "../utils/FileUtil.h"

// 构造函数：初始化服务器配置
HttpServer::HttpServer(int port, const std::string& root_dir,
                       int thread_count, int keep_alive_timeout)
    : port_(port), root_dir_(root_dir),
      thread_count_(thread_count), keep_alive_timeout_(keep_alive_timeout),
      running_(false) {
}

// 析构函数：停止服务器并清理资源
HttpServer::~HttpServer() {
    stop();
    join();
}

// 启动HTTP服务器（非阻塞：初始化完成后立即返回，事件循环在独立线程中运行）
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

    // 创建并启动线程池
    // 线程数量从配置读取，不再硬编码
    thread_pool_ = std::make_unique<ThreadPool>(thread_count_);
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

    // 将事件循环放到独立线程中运行，start() 不再阻塞
    // main() 因此有机会轮询 g_running 信号标志并触发优雅关闭
    event_loop_thread_ = std::make_unique<std::thread>(&HttpServer::runEventLoop, this);

    return true;
}

// 事件循环线程入口
// 内容与原 start() 中的 while 循环完全一致：1秒超时的 epoll_wait + 超时检查
void HttpServer::runEventLoop() {
    std::vector<epoll_event> events;
    while (running_) {
        int nfds = epoll_->wait(events, 1000);  // 1 秒超时
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (nfds > 0) {
            epoll_->processEvents(events, nfds);
        }
        // 每次 epoll_wait 返回后检查一次空闲连接超时
        checkTimeouts();
    }
}

// 等待事件循环线程退出
void HttpServer::join() {
    if (event_loop_thread_ && event_loop_thread_->joinable()) {
        event_loop_thread_->join();
    }
}

// 停止HTTP服务器
void HttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // 关闭所有客户端连接（hand-over-hand：锁内收集fd，锁外close）
    std::vector<int> close_fds;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& pair : connections_) {
            if (pair.second.fd >= 0) {
                close_fds.push_back(pair.second.fd);
            }
        }
        connections_.clear();
        fd_to_conn_id_.clear();
    }
    for (int fd : close_fds) {
        ::close(fd);
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
            if (errno == EINTR) {
                continue;                            // 被信号中断，重试 accept
            }
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

        // 分配单调递增的连接ID（原子操作，无需加锁）
        uint64_t conn_id = next_conn_id_.fetch_add(1);

        // 添加到连接列表
        Connection conn;                             // conn: 记录这个客户端的"档案卡"
        conn.conn_id = conn_id;                      // 档案卡上记下唯一连接ID
        conn.fd = client_fd;                         // 档案卡上记下身份证号
        conn.buffer_len = 0;                         // 当前还没收到数据，缓冲区长度 0
        conn.keep_alive = false;                     // 暂不支持长连接
        conn.last_active = time(nullptr);            // 记录当前时间为最近活跃时间
        {
            std::lock_guard<std::mutex> lock(mutex_); // lock: 上锁，防止多个线程同时修改列表
            connections_.emplace(conn_id, conn);      // 以 conn_id 为 key 存入哈希表
            fd_to_conn_id_.emplace(client_fd, conn_id); // fd → conn_id 反向映射
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
        // 通过 fd 查找 conn_id 后按 ID 关闭，避免 fd 复用问题
        uint64_t conn_id = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = fd_to_conn_id_.find(client_fd);
            if (it != fd_to_conn_id_.end()) {
                conn_id = it->second;
            }
        }
        if (conn_id != 0) {
            closeConnection(conn_id);
        }
        return;
    }

    buffer[bytes_read] = '\0';
    std::string request_data(buffer, bytes_read);

    std::cout << "Received request from fd " << client_fd << std::endl;

    // 通过 fd 查找 conn_id，后续所有操作使用 conn_id 而非裸 fd
    uint64_t conn_id = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto fd_it = fd_to_conn_id_.find(client_fd);
        if (fd_it == fd_to_conn_id_.end()) {
            return;  // 连接已关闭，忽略此请求
        }
        conn_id = fd_it->second;

        auto it = connections_.find(conn_id);
        if (it == connections_.end()) {
            return;  // 连接已关闭，忽略此请求
        }
        Connection& conn = it->second;
        conn.last_active = time(nullptr);

        // 解析请求头中的 Connection 字段
        // 查找 "Connection:" 的位置
        size_t conn_pos = request_data.find("Connection:");
        if (conn_pos != std::string::npos) {
            // 从 "Connection:" 之后提取值部分
            size_t value_start = conn_pos + 11;  // "Connection:" 长度为11
            // 跳过空格和\r\n
            while (value_start < request_data.size() &&
                   (request_data[value_start] == ' ' || request_data[value_start] == '\t')) {
                ++value_start;
            }
            // 提取到行尾（\r\n）
            size_t value_end = request_data.find("\r\n", value_start);
            std::string conn_value = request_data.substr(value_start, value_end - value_start);
            // 判断是否为 keep-alive（不区分大小写）
            std::string conn_value_lower = conn_value;
            std::transform(conn_value_lower.begin(), conn_value_lower.end(),
                           conn_value_lower.begin(), ::tolower);
            conn.keep_alive = (conn_value_lower == "keep-alive");
        } else {
            // 没有 Connection 头时，根据 HTTP 版本决定默认行为
            // HTTP/1.1 默认使用长连接（persistent connection），
            // HTTP/1.0 默认使用短连接
            std::string version = "HTTP/1.0";  // 默认 HTTP/1.0
            size_t ver_start = request_data.find("HTTP/");
            if (ver_start != std::string::npos) {
                size_t ver_end = request_data.find("\r\n", ver_start);
                version = request_data.substr(ver_start, ver_end - ver_start);
            }
            conn.keep_alive = (version == "HTTP/1.1");
        }
    }

    // 将请求处理提交到线程池
    // 工作线程将执行：解析请求、读取文件、构建响应、发送响应、关闭连接
    // 传递 conn_id 而非裸 fd，避免 ABA 问题
    thread_pool_->enqueue([this, conn_id, request_data]() {
        processRequest(conn_id, request_data);
    });
}

// 在线程池中处理请求（解析、文件读取、响应构建、发送）
void HttpServer::processRequest(uint64_t conn_id, const std::string& request_data) {
    // 通过 conn_id 查找连接，若连接已关闭则直接返回（ABA 防护）
    int client_fd = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(conn_id);
        if (it == connections_.end()) {
            return;  // 连接已关闭，丢弃此请求
        }
        client_fd = it->second.fd;
    }

    // 解析HTTP请求
    HttpRequest http_request;
    //parse是解析函数，返回布尔值，解析的结果存在HttpRequest对象中
    if (!http_request.parse(request_data)) {
        handleError(conn_id, 400);
        // 检查是否为长连接，若是则不关闭连接
        bool keep_alive = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(conn_id);
            if (it != connections_.end()) {
                keep_alive = it->second.keep_alive;
            }
        }
        if (!keep_alive) {
            closeConnection(conn_id);
        }
        return;
    }

    // 校验 HTTP 方法，仅支持 GET 请求，其他方法（POST/PUT/DELETE 等）一律返回 405。
    if (http_request.method() != "GET") {
        HttpResponse response = HttpResponse::methodNotAllowed();

        bool keep_alive = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(conn_id);
            if (it != connections_.end()) {
                keep_alive = it->second.keep_alive;
            }
        }
        response.setKeepAlive(keep_alive);

        // 手动构造响应以包含 Allow 头（toString() 不支持自定义头）
        std::string resp_str = "HTTP/1.1 405 Method Not Allowed\r\n";
        resp_str += "Allow: GET\r\n";
        resp_str += "Content-Type: text/html; charset=utf-8\r\n";
        resp_str += "Content-Length: " + std::to_string(response.body().size()) + "\r\n";
        resp_str += keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
        resp_str += "\r\n";
        resp_str += response.body();

        size_t total_sent = 0;
        while (total_sent < resp_str.size()) {
            ssize_t bytes_sent = send(client_fd, resp_str.c_str() + total_sent,
                                      resp_str.size() - total_sent, MSG_NOSIGNAL);
            if (bytes_sent > 0) {
                total_sent += bytes_sent;
            } else if (bytes_sent == 0) {
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                break;
            }
        }

        if (!keep_alive) {
            closeConnection(conn_id);
        }
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

    // 从连接列表中获取 keep_alive 标志
    bool keep_alive = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(conn_id);
        if (it != connections_.end()) {
            keep_alive = it->second.keep_alive;
        }
    }

    // 使用FileUtil读取文件
    FileUtil file_util(full_path);
    if (!file_util.readFile()) {
        std::cerr << "File not found: " << full_path << std::endl;
        handleError(conn_id, 404);
        ServerLogger::getInstance().log(http_request.method(), file_path, 404, 0);
        if (!keep_alive) {
            closeConnection(conn_id);
        }
        return;
    }

    // 生成HTTP响应
    HttpResponse response;
    // 设置响应状态码为200（成功）
    response.setStatusCode(200);
    // 设置响应内容类型（根据文件路径）
    response.setContentTypeByPath(file_path);
    // 设置是否长连接
    response.setKeepAlive(keep_alive);
    // 设置响应体（文件内容）
    response.setBody(file_util.content());

    // 通过 stat() 获取文件最后修改时间，设置 Last-Modified 响应头
    struct stat file_stat;
    if (stat(full_path.c_str(), &file_stat) == 0) {
        response.setLastModified(file_stat.st_mtime);
    }

    // 根据文件扩展名设置不同的 Cache-Control 策略
    // HTML -> no-cache；图片/CSS/JS -> public, max-age=3600
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos != std::string::npos) {
        std::string ext = file_path.substr(dot_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "html" || ext == "htm") {
            response.setCacheControl("no-cache");
        } else if (ext == "css" || ext == "js" ||
                   ext == "png" || ext == "jpg" || ext == "jpeg" ||
                   ext == "gif" || ext == "bmp" || ext == "ico" || ext == "svg") {
            response.setCacheControl("public, max-age=3600");
        }
    }

    // 发送响应
    sendResponse(client_fd, response);

    // 记录访问日志
    ServerLogger::getInstance().log(http_request.method(), file_path, 200,
                                    response.toString().size());

    // 根据 keep_alive 决定是否关闭连接
    // 如果客户端请求长连接，不关闭连接，等待下一个请求
    // 空闲连接将由 checkTimeouts() 超时断开机制自动关闭
    if (keep_alive) {
        std::cout << "Keep-alive connection, waiting for next request on fd "
                  << client_fd << std::endl;
    } else {
        // 短连接：响应完成后关闭连接
        closeConnection(conn_id);
    }
}

// 发送响应给客户端
void HttpServer::sendResponse(int client_fd, const HttpResponse& response) {
    std::string resp_str = response.toString();
    size_t total_sent = 0;

    while (total_sent < resp_str.size()) {
        ssize_t bytes_sent = send(client_fd, resp_str.c_str() + total_sent,
                                  resp_str.size() - total_sent, MSG_NOSIGNAL);

        if (bytes_sent > 0) {
            total_sent += bytes_sent;
        } else if (bytes_sent == 0) {
            // 对端关闭连接
            break;
        } else {  // bytes_sent < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下缓冲区已满，重试
                continue;
            } else {
                // ECONNRESET 或其他致命错误
                std::cerr << "Error sending response to client: "
                          << strerror(errno) << std::endl;
                break;
            }
        }
    }
}

// 关闭客户端连接（按 conn_id 删除）
void HttpServer::closeConnection(uint64_t conn_id) {
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = connections_.find(conn_id);
        if (it == connections_.end()) {
            return;  // 连接已关闭或不存在
        }

        fd = it->second.fd;

        // 从epoll中移除
        if (epoll_) {
            epoll_->remove(fd);
        }

        // 从连接列表中移除（O(1) 哈希表删除）
        connections_.erase(conn_id);
        // 同时清理 fd → conn_id 反向映射
        fd_to_conn_id_.erase(fd);
    }

    // 关闭socket（不在锁内执行，避免不必要的持锁时间）
    if (fd >= 0) {
        ::close(fd);
    }

    std::cout << "Connection closed: conn_id " << conn_id << ", fd " << fd << std::endl;
    ServerLogger::getInstance().log("连接关闭，conn_id：" + std::to_string(conn_id) +
                                    "，fd：" + std::to_string(fd));
}

// 错误处理：生成错误响应（不关闭连接，由调用方决定）
void HttpServer::handleError(uint64_t conn_id, int status_code) {
    int client_fd = -1;
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

    // 从连接列表中获取 keep_alive 标志和 fd，设置到错误响应中
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(conn_id);
        if (it != connections_.end()) {
            response.setKeepAlive(it->second.keep_alive);
            client_fd = it->second.fd;
        }
    }

    if (client_fd >= 0) {
        sendResponse(client_fd, response);
    }
}

// 检查并关闭超时空闲连接
void HttpServer::checkTimeouts() {
    time_t now = time(nullptr);
    std::vector<int> timeout_fds;  // 收集需要关闭的 fd

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = connections_.begin(); it != connections_.end(); ) {
            // 如果 last_active 非零且空闲时间超过阈值
            if (it->second.last_active > 0 &&
                (now - it->second.last_active) >= keep_alive_timeout_) {
                int fd = it->second.fd;  // 在 erase 前保存 fd
                timeout_fds.push_back(fd);

                // 先从 epoll 中移除
                if (epoll_) {
                    epoll_->remove(fd);
                }
                // 清理 fd → conn_id 反向映射（必须在 erase 前执行）
                fd_to_conn_id_.erase(fd);
                // 从连接列表中删除
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 关闭超时连接的 socket（不在锁内执行）
    for (int fd : timeout_fds) {
        ::close(fd);
        std::cout << "Connection timeout, closed: fd " << fd << std::endl;
        ServerLogger::getInstance().log("连接超时关闭，fd：" + std::to_string(fd));
    }
}
