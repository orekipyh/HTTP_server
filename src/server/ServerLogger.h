// ServerLogger.h - 服务器日志记录器（单例模式）
#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

class ServerLogger {
public:
    // 获取全局唯一实例
    static ServerLogger& getInstance();

    // 记录一条访问日志
    // method: HTTP 方法（GET/POST 等）
    // path: 请求路径
    // status_code: 响应状态码
    // response_size: 响应体大小（字节）
    void log(const std::string& method, const std::string& path,
             int status_code, size_t response_size);

    // 记录一条通用消息日志（用于连接关闭、超时等非请求事件）
    void log(const std::string& message);

    // 禁止拷贝和赋值，确保单例唯一性
    ServerLogger(const ServerLogger&) = delete;
    ServerLogger& operator=(const ServerLogger&) = delete;

private:
    // 私有构造函数，外部只能通过 getInstance() 获取实例
    // log_file: 日志文件名，默认为 server.log
    ServerLogger(const std::string& log_file = "server.log");
    ~ServerLogger();

    std::ofstream log_file_;   // 日志文件输出流
    std::mutex mutex_;          // 保护文件写入的互斥锁
};

#endif // SERVER_LOGGER_H
