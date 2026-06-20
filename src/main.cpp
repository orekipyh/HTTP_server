#include <iostream>
#include <cstdlib>
#include <cstring>
#include "server/HttpServer.h"

// 打印使用方法
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <port> <root_directory>" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  port            服务器监听端口（如 8080）" << std::endl;
    std::cout << "  root_directory  静态资源根目录（如 ../www/）" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " 8080 ../www/" << std::endl;
}

int main(int argc, char* argv[]) {
    // 检查命令行参数数量
    // 必须提供端口号和静态资源目录两个参数
    if (argc != 3) {
        std::cerr << "Error: Invalid number of arguments" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // 解析端口号
    // atoi将字符串转换为整数，如果转换失败返回0
    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "Error: Invalid port number (must be between 1 and 65535)" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // 获取静态资源目录
    // 使用strdup避免原始指针问题，简化处理
    std::string root_dir = argv[2];

    // 确保root_dir以'/'结尾（如果非空）
    if (!root_dir.empty() && root_dir.back() != '/') {
        root_dir += '/';
    }

    std::cout << "========================================" << std::endl;
    std::cout << "   C++ HTTP Static File Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Port:        " << port << std::endl;
    std::cout << "Root Dir:    " << root_dir << std::endl;
    std::cout << "========================================" << std::endl;

    // 创建HttpServer实例
    HttpServer server(port, root_dir);

    // 启动服务器
    // start()会阻塞当前线程，直到服务器停止
    if (!server.start()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return 1;
    }

    return 0;
}
