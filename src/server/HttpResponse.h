// HttpResponse.h - HTTP响应生成类头文件
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <map>

class HttpResponse {
public:
    // 构造函数
    HttpResponse();

    // 设置HTTP状态码
    // code: HTTP状态码（200/404/500等）
    void setStatusCode(int code);

    // 设置Content-Type响应头
    // content_type: MIME类型（如"text/html"、"image/png"等）
    void setContentType(const std::string& content_type);

    // 设置响应体内容
    // body: 响应体内容（HTML内容、文件内容等）
    void setBody(const std::string& body);

    // 根据文件扩展名自动设置Content-Type
    // file_path: 文件路径（如"/index.html"）
    // 返回: 是否成功设置
    bool setContentTypeByPath(const std::string& file_path);

    // 生成完整的HTTP响应字符串
    // 格式: "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 123\r\n\r\n<html>..."
    std::string toString() const;

    // 获取状态码
    int statusCode() const { return status_code_; }

    // 获取Content-Type
    const std::string& contentType() const { return content_type_; }

    // 获取响应体
    const std::string& body() const { return body_; }

    // 生成404 Not Found错误响应
    // 返回: 包含HTML错误页面的HttpResponse对象
    static HttpResponse notFound();

    // 生成500 Internal Server Error错误响应
    // 返回: 包含HTML错误页面的HttpResponse对象
    static HttpResponse internalError();

private:
    // 根据扩展名获取MIME类型
    // ext: 文件扩展名（如"html"、"css"、"png"）
    // 返回: 对应的MIME类型
    static std::string getMimeType(const std::string& ext);

    int status_code_;                          // HTTP状态码
    std::string content_type_;                  // Content-Type
    std::string body_;                         // 响应体
    std::map<int, std::string> status_text_;  // 状态码描述映射表
};

#endif // HTTP_RESPONSE_H
