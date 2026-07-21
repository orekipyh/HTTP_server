// HttpResponse.h - HTTP响应生成类头文件
#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <map>
#include <ctime>

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

    // 设置是否使用长连接（Keep-Alive）
    // keep_alive: true 表示响应头中设置 Connection: keep-alive
    void setKeepAlive(bool keep_alive);

    // 设置 Last-Modified 响应头
    // t: 文件的最后修改时间（time_t），会自动格式化为 HTTP 标准日期格式
    void setLastModified(time_t t);

    // 设置 Cache-Control 响应头
    // cache_control: 缓存控制策略（如 "no-cache"、"public, max-age=3600" 等）
    void setCacheControl(const std::string& cache_control);

    // 获取是否使用长连接
    bool keepAlive() const { return keep_alive_; }

    // 生成404 Not Found错误响应
    // 返回: 包含HTML错误页面的HttpResponse对象
    static HttpResponse notFound();

    // 生成500 Internal Server Error错误响应
    // 返回: 包含HTML错误页面的HttpResponse对象
    static HttpResponse internalError();

    // 生成405 Method Not Allowed错误响应
    // 返回: 包含HTML错误页面的HttpResponse对象
    static HttpResponse methodNotAllowed();

private:
    // 根据扩展名获取MIME类型
    // ext: 文件扩展名（如"html"、"css"、"png"）
    // 返回: 对应的MIME类型
    static std::string getMimeType(const std::string& ext);

    int status_code_;                          // HTTP状态码
    std::string content_type_;                  // Content-Type
    std::string body_;                         // 响应体
    bool keep_alive_;                          // 是否使用长连接（Keep-Alive）
    std::string last_modified_;                // Last-Modified 响应头（HTTP 标准日期格式）
    std::string cache_control_;                // Cache-Control 响应头
    std::map<int, std::string> status_text_;  // 状态码描述映射表
};

#endif // HTTP_RESPONSE_H
