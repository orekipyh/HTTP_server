// HttpServer.h - HTTP服务器主类头文件
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <sys/epoll.h>

class Socket;
class Epoll;
class HttpRequest;
class HttpResponse;
class ThreadPool;

// 客户端连接信息结构体
// 用于管理每个客户端连接的状态
struct Connection {
    int fd;                     // 客户端socket文件描述符
    char buffer[4096];         // 接收缓冲区
    int buffer_len;            // 缓冲区中数据长度
    bool keep_alive;           // 是否保持连接

    Connection() : fd(-1), buffer_len(0), keep_alive(false) {
        buffer[0] = '\0';
    }
};

class HttpServer {
public:
    // 构造函数
    // port: 服务器监听端口
    // root_dir: 静态资源根目录
    HttpServer(int port, const std::string& root_dir);

    // 析构函数
    ~HttpServer();

    // 启动HTTP服务器
    // 包含初始化socket、epoll，启动事件循环
    bool start();

    // 停止HTTP服务器
    void stop();

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
    void processRequest(int client_fd, const std::string& request_data);

    // 发送响应给客户端
    void sendResponse(int client_fd, const HttpResponse& response);

    // 关闭客户端连接
    void closeConnection(int client_fd);

    // 错误处理
    void handleError(int client_fd, int status_code);

    int port_;                              // 监听端口
    std::string root_dir_;                  // 静态资源根目录
    std::unique_ptr<Socket> server_socket_; // 服务器socket
    std::unique_ptr<Epoll> epoll_;         // epoll多路复用器
    std::unique_ptr<ThreadPool> thread_pool_; // 线程池
    std::vector<Connection> connections_;  // 客户端连接列表
    std::mutex mutex_;                    // 保护connections_和epoll操作的互斥锁
    bool running_;                         // 服务器运行状态
};

#endif // HTTP_SERVER_H
