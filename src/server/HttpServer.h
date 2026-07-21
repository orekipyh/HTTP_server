// HttpServer.h - HTTP服务器主类头文件
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <ctime>
#include <sys/epoll.h>

class Socket;
class Epoll;
class HttpRequest;
class HttpResponse;
class ThreadPool;

// 客户端连接信息结构体
// 用于管理每个客户端连接的状态
struct Connection {
    uint64_t conn_id;            // 单调递增的连接ID（用于ABA防护）
    int fd;                     // 客户端socket文件描述符
    char buffer[4096];         // 接收缓冲区
    int buffer_len;            // 缓冲区中数据长度
    bool keep_alive;           // 是否保持连接
    time_t last_active;        // 最近活跃时间（用于超时断开检查）

    Connection() : conn_id(0), fd(-1), buffer_len(0), keep_alive(false), last_active(0) {
        buffer[0] = '\0';
    }
};

class HttpServer {
public:
    // 构造函数
    // port: 服务器监听端口
    // root_dir: 静态资源根目录
    // thread_count: 线程池工作线程数
    // keep_alive_timeout: 空闲连接超时时间（秒）
    HttpServer(int port, const std::string& root_dir,
               int thread_count = 5, int keep_alive_timeout = 3600);

    // 析构函数
    ~HttpServer();

    // 启动HTTP服务器
    // 包含初始化socket、epoll，启动事件循环
    bool start();

    // 停止HTTP服务器
    void stop();

    // 等待事件循环线程退出
    void join();

    // 获取监听端口
    int port() const { return port_; }

    // 获取静态资源根目录
    const std::string& rootDir() const { return root_dir_; }

private:
    // 初始化服务器socket
    // 创建socket、绑定端口、开始监听
    bool initSocket();

    // 初始化epoll
    // 创建epoll实例，注册监听socket的读事件
    bool initEpoll();

    // 处理新连接
    // 当监听socket可读时调用（表示有新连接到来）
    void handleNewConnection();

    // 处理客户端请求
    // 当客户端socket可读时调用（表示有数据到来）
    void handleClientRequest(int client_fd);

    // 在线程池中处理请求（解析、文件读取、响应构建、发送、关闭）
    void processRequest(uint64_t conn_id, const std::string& request_data);

    // 发送响应给客户端
    void sendResponse(int client_fd, const HttpResponse& response);

    // 关闭客户端连接（按 conn_id 删除）
    void closeConnection(uint64_t conn_id);

    // 错误处理
    void handleError(uint64_t conn_id, int status_code);

    // 超时连接检查，关闭长时间空闲的连接
    void checkTimeouts();

    // 事件循环线程入口（独立线程中执行）
    void runEventLoop();

    int port_;                              // 监听端口
    std::string root_dir_;                  // 静态资源根目录
    int thread_count_;                      // 线程池工作线程数
    int keep_alive_timeout_;                // 空闲连接超时时间（秒）
    std::unique_ptr<Socket> server_socket_; // 服务器socket
    std::unique_ptr<Epoll> epoll_;         // epoll多路复用器
    std::unique_ptr<ThreadPool> thread_pool_; // 线程池
    std::unique_ptr<std::thread> event_loop_thread_; // 事件循环线程
    std::unordered_map<uint64_t, Connection> connections_;  // 客户端连接列表（conn_id → Connection）
    std::unordered_map<int, uint64_t> fd_to_conn_id_;       // fd → conn_id 反向映射
    std::mutex mutex_;                    // 保护connections_和epoll操作的互斥锁
    bool running_;                         // 服务器运行状态
    std::atomic<uint64_t> next_conn_id_{1}; // 单调递增的连接ID原子计数器
};

#endif // HTTP_SERVER_H
