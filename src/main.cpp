#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <unistd.h>
#include "server/HttpServer.h"

// 全局原子标志：信号处理器置 false，main() 轮询该标志后触发优雅关闭
// C++11 起 std::atomic 保证多线程下的可见性与原子性
std::atomic<bool> g_running{true};

// 信号处理函数：仅做"设置标志 + 打印提示"两件最小化工作
// 信号处理器内禁止调用非异步信号安全函数（如 malloc/new/printf），
// 这里只做一次写操作到 std::atomic<bool> 和直接 write 到 stderr，是安全的
void signalHandler(int sig) {
    g_running.store(false);

    const char msg[] = "\nReceived shutdown signal, preparing to stop server...\n";
    ssize_t _ = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)_;
}

// 打印使用方法
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "       " << program_name << " <port> <root_directory>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --config <path>   从 JSON 配置文件读取服务器参数" << std::endl;
    std::cout << "  --help            显示此帮助信息" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments (传统方式):" << std::endl;
    std::cout << "  port            服务器监听端口（如 8080）" << std::endl;
    std::cout << "  root_directory  静态资源根目录（如 ../www/）" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " --config ../config.json" << std::endl;
    std::cout << "  " << program_name << " 8080 ../www/" << std::endl;
}

// 简单的 JSON 键值提取工具（仅支持扁平 JSON 对象）
// 从 JSON 字符串中提取指定 key 的字符串值（去除首尾引号）
static std::string getJsonStringValue(const std::string& json, const std::string& key) {
    // 查找 "key":
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        return "";
    }

    // 定位到值起始位置
    pos += search.size();

    // 跳过空白字符
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
        ++pos;
    }

    if (pos >= json.size()) {
        return "";
    }

    // 如果是字符串值（以引号开头）
    if (json[pos] == '"') {
        ++pos;  // 跳过开头的引号
        std::string value;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                // 处理转义字符
                ++pos;
                if (json[pos] == '"') value += '"';
                else if (json[pos] == '\\') value += '\\';
                else if (json[pos] == 'n') value += '\n';
                else if (json[pos] == 't') value += '\t';
                else value += json[pos];
            } else {
                value += json[pos];
            }
            ++pos;
        }
        return value;
    }

    // 如果是数字值
    std::string value;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-')) {
        value += json[pos];
        ++pos;
    }
    return value;
}

// 从 JSON 文件中读取配置
// 返回值：true 表示成功读取配置，false 表示出错
static bool readConfigFromFile(const std::string& config_path,
                                int& port,
                                std::string& root_dir,
                                int& thread_count,
                                int& keep_alive_timeout) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << config_path << std::endl;
        return false;
    }

    // 读取整个文件内容
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    // 提取各字段
    std::string port_str = getJsonStringValue(json, "port");
    std::string root_dir_str = getJsonStringValue(json, "root_dir");
    std::string thread_count_str = getJsonStringValue(json, "thread_count");
    std::string keep_alive_timeout_str = getJsonStringValue(json, "keep_alive_timeout");

    // 校验必填字段
    if (port_str.empty() || root_dir_str.empty()) {
        std::cerr << "Error: Config file must contain 'port' and 'root_dir' fields" << std::endl;
        return false;
    }

    port = std::atoi(port_str.c_str());
    if (port <= 0 || port > 65535) {
        std::cerr << "Error: Invalid port number in config (must be between 1 and 65535)" << std::endl;
        return false;
    }

    root_dir = root_dir_str;

    // 相对路径归一化：如果 root_dir 是相对路径（不以 / 开头），
    // 将其拼接到"配置文件所在的目录"而非"当前工作目录"。
    //
    // 例1: config_path = "../config.json" , root_dir = "./www"
    //       → config_dir = "..", 拼接后 root_dir = ".././www"
    //       （指向项目根目录下的 www，而非 CWD 下的 www）
    //
    // 例2: config_path = "/home/user/project/config.json", root_dir = "./www"
    //       → config_dir = "/home/user/project", 拼接后 root_dir = "/home/user/project/./www"
    //
    // 例3: config_path = "config.json" （仅文件名，无目录分隔符）
    //       → config_dir = "." ，拼接后 root_dir = "./www"（等同于改前行为）
    if (!root_dir.empty() && root_dir[0] != '/') {
        std::string config_dir = config_path;
        // 找到最后一个 '/' 或 '\\'，去掉文件名部分，得到配置文件所在目录
        size_t pos = config_dir.find_last_of("/\\");
        if (pos != std::string::npos) {
            config_dir = config_dir.substr(0, pos);
        } else {
            config_dir = ".";
        }
        root_dir = config_dir + "/" + root_dir;
    }

    // 可选字段：thread_count，默认 5
    if (!thread_count_str.empty()) {
        thread_count = std::atoi(thread_count_str.c_str());
        if (thread_count <= 0) {
            std::cerr << "Error: Invalid thread_count in config" << std::endl;
            return false;
        }
    }

    // 可选字段：keep_alive_timeout，默认 30
    if (!keep_alive_timeout_str.empty()) {
        keep_alive_timeout = std::atoi(keep_alive_timeout_str.c_str());
        if (keep_alive_timeout <= 0) {
            std::cerr << "Error: Invalid keep_alive_timeout in config" << std::endl;
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[]) {
    // 在 main() 最开始注册 SIGINT 和 SIGTERM 信号处理器
    // 确保无论后续逻辑如何，按 Ctrl+C 或 kill 进程时都能捕获到信号
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    int port = 0;
    std::string root_dir;
    int thread_count = 5;
    int keep_alive_timeout = 30;

    // 检查是否使用 --config 方式
    if (argc == 3 && std::strcmp(argv[1], "--config") == 0) {
        // 从配置文件读取
        if (!readConfigFromFile(argv[2], port, root_dir, thread_count, keep_alive_timeout)) {
            return 1;
        }
    } else if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        printUsage(argv[0]);
        return 0;
    } else if (argc == 3) {
        // 传统命令行参数方式
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Error: Invalid port number (must be between 1 and 65535)" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        root_dir = argv[2];
    } else {
        std::cerr << "Error: Invalid number of arguments" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // 确保 root_dir 以 '/' 结尾
    if (!root_dir.empty() && root_dir.back() != '/') {
        root_dir += '/';
    }

    std::cout << "========================================" << std::endl;
    std::cout << "   C++ HTTP Static File Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Port:        " << port << std::endl;
    std::cout << "Root Dir:    " << root_dir << std::endl;
    std::cout << "Threads:     " << thread_count << std::endl;
    std::cout << "Keep-Alive:  " << keep_alive_timeout << "s" << std::endl;
    std::cout << "========================================" << std::endl;

    // 创建 HttpServer 实例（传入线程数和超时时间）
    HttpServer server(port, root_dir, thread_count, keep_alive_timeout);

    if (!server.start()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return 1;
    }

    // start() 是非阻塞的（事件循环在独立线程中）
    // 主线程在此轮询 g_running 标志，每秒检查一次是否收到信号
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 信号已收到，触发优雅关闭
    std::cout << "Initiating graceful shutdown..." << std::endl;

    // 先 stop()：关闭所有连接、停止 epoll、停止线程池
    // stop() 内部会将 running_ 置为 false，event_loop_thread_ 会因 while(running_) 退出
    server.stop();

    // 再 join()：确保事件循环线程真正完全退出，避免主函数返回时对象析构顺序问题
    server.join();

    std::cout << "Server shutdown complete, goodbye." << std::endl;

    return 0;
}
