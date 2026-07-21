#include "HttpRequest.h"
#include <algorithm>
#include <cctype>
#include <sstream>

// 构造函数：初始化空的请求对象
HttpRequest::HttpRequest() {
    clear();
}

// 清空请求数据
void HttpRequest::clear() {
    method_.clear();
    path_.clear();
    version_.clear();
    headers_.clear();
}

// 解析HTTP请求字符串
// 格式: "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
// 请求行
// 请求头
// 请求体(可选)
bool HttpRequest::parse(const std::string& request) {
    // 如果请求为空，解析失败
    if (request.empty()) {
        return false;
    }

    // 将请求字符串按行分割
    // HTTP协议使用\r\n作为行分隔符
    std::istringstream stream(request);
    std::string line;

    // 读取第一行（请求行）
    if (std::getline(stream, line)) {
        // 移除可能的\r字符（Windows风格行尾）
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // 解析请求行GET /index.html HTTP/1.1
        // 拆解成method=GET, path=/index.html, version=HTTP/1.1
        if (!parseRequestLine(line)) {
            return false;
        }
    } else {
        return false;
    }

    // 解析请求头
    while (std::getline(stream, line)) {
        // 移除\r字符
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 空行表示请求头结束（\r\n\r\n）
        if (line.empty()) {
            break;
        }

        // 解析请求头行Host: localhost:8080
        // 拆解成name=Host, value=localhost:8080
        if (!parseHeader(line)) {
            return false;
        }
    }

    // 解析请求体（根据 Content-Length 精确读取）
    // 请求体是原始二进制数据，不能用 getline 逐行读（会破坏二进制内容和换行符）
    std::string content_length_str = getHeader("Content-Length");
    if (!content_length_str.empty()) {
        try {
            size_t body_size = std::stoul(content_length_str);
            if (body_size > 0) {
                // 从 stream 当前位置读取指定字节数的请求体
                // 此时 stream 指针已越过 \r\n\r\n，指向请求体的起始位置
                // 创建一个长度为body_size的字符串每个字符初始化为\0用于存储请求体内容
                std::string body_content(body_size, '\0');
                // 从 stream 当前位置读取 body_size 字个字节到 body_content
                stream.read(&body_content[0], body_size);

                // 检查是否读到了足够的字节
                if (static_cast<size_t>(stream.gcount()) != body_size) {
                    return false;
                }

                if (!parseBody(body_content)) {
                    return false;
                }
            }
        } catch (...) {
            return false;
        }
    }

    return true;
}

// 解析请求行
// 格式: "GET /index.html HTTP/1.1"
bool HttpRequest::parseRequestLine(const std::string& line) {
    std::istringstream stream(line);
    std::string segment;

    // 按空格分割请求行
    // 第一部分：HTTP方法（GET/POST等）
    if (!(stream >> segment)) {
        return false;
    }
    method_ = segment;

    // 第二部分：请求路径
    if (!(stream >> segment)) {
        return false;
    }
    path_ = segment;

    // 第三部分：HTTP版本
    if (!(stream >> segment)) {
        return false;
    }
    version_ = segment;

    // 验证解析结果是否有效
    // method_和version_不应为空
    if (method_.empty() || version_.empty()) {
        return false;
    }

    return true;
}

// 解析请求头
// 格式: "Host: localhost:8080"
bool HttpRequest::parseHeader(const std::string& header_line) {
    // 查找冒号的位置
    size_t colon_pos = header_line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    // 提取字段名（冒号前的部分）
    std::string name = header_line.substr(0, colon_pos);
    // 提取字段值（冒号后的部分）
    std::string value = header_line.substr(colon_pos + 1);

    // 去除字段名和值两端的空白字符
    trim(name);
    trim(value);

    // 将字段名转换为小写（HTTP头不区分大小写）
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // 存储到headers_映射表中
    headers_[name] = value;

    return true;
}

// 解析请求体
// body: 原始请求体的完整内容（基于 Content-Length 精确读取）
bool HttpRequest::parseBody(const std::string& body) {
    if (body.empty()) {
        return false;
    }
    body_ = body;
    return true;
}

// 获取请求头字段值
// name: 请求头名称（不区分大小写）
// 返回: 对应的值，如果不存在则返回空字符串
const std::string& HttpRequest::getHeader(const std::string& name) const {
    // 创建一个临时的小写名称用于查找
    static const std::string empty;
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = headers_.find(lower_name);
    if (it != headers_.end()) {
        return it->second;
    }
    return empty;
}

// 检查请求头是否存在
bool HttpRequest::hasHeader(const std::string& name) const {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return headers_.find(lower_name) != headers_.end();
}

// 去除字符串两端的空白字符
// 包括空格、制表符、换行符等
void HttpRequest::trim(std::string& str) {
    // 去除左端空白
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                          [](unsigned char ch) { return !std::isspace(ch); }));

    // 去除右端空白
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           [](unsigned char ch) { return !std::isspace(ch); })
                  .base(),
              str.end());
}

// 规范化请求路径，防止目录遍历攻击
// 处理逻辑：
//   1. 检测路径中是否包含 ".."（目录遍历攻击）
//   2. 如果包含 ".." 则返回 "/"（拒绝访问）
//   3. 如果路径是 "/" 则返回 "/index.html"（默认页面）
//   4. 其他路径直接返回
std::string HttpRequest::normalizePath() const {
    // 空路径返回默认页面
    if (path_.empty()) {
        return "/index.html";
    }

    // 检查是否存在目录遍历攻击（路径中包含 ".."）
    // 例如："/../etc/passwd" 会尝试访问系统文件
    if (path_.find("..") != std::string::npos) {
        // 检测到目录遍历攻击，返回根路径（拒绝访问）
        return "/";
    }

    // 如果是根路径，返回默认页面 index.html
    if (path_ == "/") {
        return "/index.html";
    }

    // 其他路径直接返回，保持不变
    return path_;
}
