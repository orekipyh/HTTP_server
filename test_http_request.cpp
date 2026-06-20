#include "server/HttpRequest.h"
#include <iostream>

int main() {
    HttpRequest req;
    bool ok = req.parse("GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");

    if (!ok) return 1;
    if (req.method() != "GET") return 2;
    if (req.path() != "/index.html") return 3;
    if (req.version() != "HTTP/1.1") return 4;
    if (req.getHeader("host") != "localhost") return 5;

    std::cout << "HttpRequest::parse() 能正确解析 HTTP 请求字符串: PASS" << std::endl;
    std::cout << "能正确提取 method(" << req.method() << "), path(" << req.path()
              << "), version(" << req.version() << ") 和请求头: PASS" << std::endl;

    req.clear();
    req.parse("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string root_path = req.normalizePath();
    bool root_ok = (root_path == "/index.html");
    std::cout << "normalizePath() 根路径测试 (\"/\" -> \"" << root_path << "\"): "
              << (root_ok ? "PASS" : "FAIL") << std::endl;

    req.clear();
    req.parse("GET /../../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n");
    std::string safe_path = req.normalizePath();
    bool safe_ok = (safe_path == "/");
    std::cout << "normalizePath() 目录遍历攻击测试 (\"/../../../etc/passwd\" -> \"" << safe_path << "\"): "
              << (safe_ok ? "PASS" : "FAIL") << std::endl;

    if (!root_ok || !safe_ok) return 6;

    std::cout << "normalizePath() 能正确处理根路径和目录遍历攻击: PASS" << std::endl;

    return 0;
}
