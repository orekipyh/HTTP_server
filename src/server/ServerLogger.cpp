// ServerLogger.cpp - 服务器日志记录器实现（单例模式）
#include "ServerLogger.h"
#include <iostream>
#include <ctime>
#include <sstream>

// 获取全局唯一实例
// 使用局部静态变量（C++11 起保证线程安全的一次性初始化）
ServerLogger& ServerLogger::getInstance() {
    static ServerLogger instance;
    return instance;
}

// 私有构造函数：打开日志文件（追加模式）
ServerLogger::ServerLogger(const std::string& log_file) {
    log_file_.open(log_file, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Warning: Failed to open log file: " << log_file << std::endl;
    }
}

// 析构函数：关闭日志文件
ServerLogger::~ServerLogger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

// 记录一条通用消息日志（用于连接关闭、超时等非请求事件）
void ServerLogger::log(const std::string& message) {
    std::time_t now = std::time(nullptr);
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::ostringstream oss;
    oss << "时间：[" << time_str << "] " << message;

    std::string log_msg = oss.str();

    // 输出到终端
    std::cout << log_msg << std::endl;

    // 写入日志文件（线程安全）
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_ << log_msg << std::endl;
        log_file_.flush();
    }
}

// 记录一条访问日志，同时输出到终端和日志文件
void ServerLogger::log(const std::string& method, const std::string& path,
                       int status_code, size_t response_size) {
    // 获取当前时间并格式化为字符串
    std::time_t now = std::time(nullptr);
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    // 组装日志消息
    std::ostringstream oss;
    oss << "时间：[" << time_str << "]"
        << " 请求方式：" << method
        << " 请求页面：" << path
        << " 状态码：" << status_code
        << " 请求长度：" << response_size << "bytes";

    std::string log_msg = oss.str();

    // 输出到终端
    std::cout << log_msg << std::endl;

    // 写入日志文件（线程安全）
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_ << log_msg << std::endl;
        log_file_.flush();
    }
}
