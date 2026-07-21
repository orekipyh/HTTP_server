// HttpRequest.h - HTTP请求解析类头文件
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <map>
#include <cstdint>

class HttpRequest {
public:
    // 构造函数
    HttpRequest();

    // 解析HTTP请求字符串
    // request: 原始HTTP请求字符串
    // 返回: 解析成功返回true，失败返回false
    bool parse(const std::string& request);

    // 获取HTTP方法（GET/POST等）
    const std::string& method() const { return method_; }

    // 获取请求路径
    const std::string& path() const { return path_; }

    // 获取HTTP版本
    const std::string& version() const { return version_; }

    // 获取请求头字段
    // name: 请求头名称
    // 返回: 对应的值，如果不存在则返回空字符串
    const std::string& getHeader(const std::string& name) const;

    // 检查请求头是否存在
    bool hasHeader(const std::string& name) const;

    // 清空请求数据，用于复用对象
    void clear();

    // 获取所有请求头
    const std::map<std::string, std::string>& headers() const { return headers_; }

    // 规范化请求路径，防止目录遍历攻击
    // 功能：
    //   - 处理路径中的 ".." 防止目录遍历攻击
    //   - 根路径 "/" 返回 "/index.html"
    //   - 正常路径保持不变
    // 返回: 规范化后的路径
    std::string normalizePath() const;

private:
    // 解析请求行（Method Path Version）
    // line: 请求行字符串
    // 返回: 解析成功返回true
    bool parseRequestLine(const std::string& line);

    // 解析请求头
    // header_line: 请求头行字符串（格式: "Name: Value"）
    // 返回: 解析成功返回true
    bool parseHeader(const std::string& header_line);

    // 解析请求体
    // body: 请求体内容
    // 返回: 解析成功返回true
    bool parseBody(const std::string& body);

    // 将字符串两端空白字符去除
    // str: 待处理的字符串
    void trim(std::string& str);

    std::string method_;                     // HTTP方法（GET/POST等）
    std::string path_;                      // 请求路径
    std::string version_;                   // HTTP版本
    std::map<std::string, std::string> headers_;  // 请求头映射表
    std::string body_;
};

#endif // HTTP_REQUEST_H
