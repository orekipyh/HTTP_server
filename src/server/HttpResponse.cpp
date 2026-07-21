#include "HttpResponse.h"
#include <sstream>
#include <algorithm>
#include <ctime>

// 构造函数：初始化状态码和状态文本映射，默认使用短连接
HttpResponse::HttpResponse() : status_code_(200), keep_alive_(false) {
    // 初始化常见HTTP状态码及其描述
    status_text_[200] = "OK";
    status_text_[404] = "Not Found";
    status_text_[405] = "Method Not Allowed";
    status_text_[500] = "Internal Server Error";
    status_text_[400] = "Bad Request";
    status_text_[403] = "Forbidden";
    status_text_[301] = "Moved Permanently";
    status_text_[302] = "Found";
}

// 设置HTTP状态码
void HttpResponse::setStatusCode(int code) {
    status_code_ = code;
}

// 设置Content-Type响应头
void HttpResponse::setContentType(const std::string& content_type) {
    content_type_ = content_type;
}

// 设置响应体内容
void HttpResponse::setBody(const std::string& body) {
    body_ = body;
}

// 设置是否使用长连接（Keep-Alive）
void HttpResponse::setKeepAlive(bool keep_alive) {
    keep_alive_ = keep_alive;
}

// 设置 Last-Modified 响应头
// 将 time_t 格式化为 HTTP 标准日期格式：如 "Thu, 01 Dec 2024 16:00:00 GMT"
void HttpResponse::setLastModified(time_t t) {
    //将 time_t 转换为 struct tm 结构体
    struct tm* gmt = gmtime(&t);
    char buf[128];
    //将 struct tm 按格式字符串 %a, %d %b %Y %H:%M:%S GMT 写入 buf ，最终得到形如 "Thu, 01 Dec 2024 16:00:00 GMT" 的字符串
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    last_modified_ = buf;
}

// 设置 Cache-Control 响应头
void HttpResponse::setCacheControl(const std::string& cache_control) {
    cache_control_ = cache_control;
}

// 根据文件路径自动设置Content-Type
bool HttpResponse::setContentTypeByPath(const std::string& file_path) {
    // 查找文件扩展名（最后一个.之后的部分）
    size_t pos = file_path.rfind('.');
    if (pos == std::string::npos || pos == file_path.length() - 1) {
        // 没有扩展名，使用默认的application/octet-stream
        content_type_ = "application/octet-stream";
        return false;
    }

    // 提取扩展名并转换为小写
    std::string ext = file_path.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // 根据扩展名设置MIME类型
    content_type_ = getMimeType(ext);
    return true;
}

// 生成完整的HTTP响应字符串
std::string HttpResponse::toString() const {
    std::ostringstream oss;

    // 生成状态行: "HTTP/1.1 200 OK"
    oss << "HTTP/1.1 " << status_code_ << " ";

    // 查找状态描述，如果不存在则使用"Unknown"
    auto it = status_text_.find(status_code_);
    if (it != status_text_.end()) {
        oss << it->second;
    } else {
        oss << "Unknown";
    }
    oss << "\r\n";

    // 生成Content-Type头
    if (!content_type_.empty()) {
        oss << "Content-Type: " << content_type_ << "\r\n";
    }

    // 生成Content-Length头（响应体大小）
    oss << "Content-Length: " << body_.size() << "\r\n";

    // 生成Connection头
    // 如果客户端请求长连接（keep-alive），则响应头中设置 Connection: keep-alive
    // 否则设置 Connection: close，通知客户端响应完成后关闭连接
    if (keep_alive_) {
        oss << "Connection: keep-alive\r\n";
    } else {
        oss << "Connection: close\r\n";
    }

    // 生成Cache-Control头（如果已设置）
    if (!cache_control_.empty()) {
        oss << "Cache-Control: " << cache_control_ << "\r\n";
    }

    // 生成Last-Modified头（如果已设置）
    if (!last_modified_.empty()) {
        oss << "Last-Modified: " << last_modified_ << "\r\n";
    }

    // 空行分隔 header 和 body
    oss << "\r\n";

    // 添加响应体
    oss << body_;

    return oss.str();
}

// 根据扩展名获取MIME类型
// 常见的MIME类型映射表
std::string HttpResponse::getMimeType(const std::string& ext) {
    // 使用静态map存储扩展名到MIME类型的映射
    // 这是一个常用的MIME类型映射
    static const std::map<std::string, std::string> mime_types = {
        // HTML
        {"html", "text/html"},
        {"htm", "text/html"},

        // CSS
        {"css", "text/css"},

        // JavaScript
        {"js", "application/javascript"},

        // JSON
        {"json", "application/json"},

        // XML
        {"xml", "application/xml"},

        // 普通文本
        {"txt", "text/plain"},

        // 图片
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"ico", "image/x-icon"},
        {"svg", "image/svg+xml"},

        // 音频
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"ogg", "audio/ogg"},

        // 视频
        {"mp4", "video/mp4"},
        {"avi", "video/x-msvideo"},
        {"mov", "video/quicktime"},
        {"wmv", "video/x-ms-wmv"},

        // 字体
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"eot", "application/vnd.ms-fontobject"},

        // 压缩文件
        {"zip", "application/zip"},
        {"gz", "application/gzip"},
        {"tar", "application/x-tar"},

        // PDF
        {"pdf", "application/pdf"},

        // 其他
        {"swf", "application/x-shockwave-flash"}
    };

    // 查找扩展名对应的MIME类型
    auto it = mime_types.find(ext);
    if (it != mime_types.end()) {
        return it->second;
    }

    // 未找到时返回默认的二进制流类型
    return "application/octet-stream";
}

// 生成404 Not Found错误响应
// 当请求的文件不存在时返回此响应
HttpResponse HttpResponse::notFound() {
    HttpResponse response;
    response.setStatusCode(404);
    response.setContentType("text/html");

    // 生成简单的HTML错误页面
    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>404 Not Found</title></head>\n"
         << "<body>\n"
         << "<h1>404 Not Found</h1>\n"
         << "<p>The requested file was not found on this server.</p>\n"
         << "</body>\n"
         << "</html>\n";

    response.setBody(body.str());
    return response;
}

// 生成405 Method Not Allowed错误响应
// 当请求方法不是GET时返回此响应，并附带 Allow: GET 响应头
HttpResponse HttpResponse::methodNotAllowed() {
    HttpResponse response;
    response.setStatusCode(405);
    response.setContentType("text/html");

    // 生成简单的HTML错误页面
    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>405 Method Not Allowed</title></head>\n"
         << "<body>\n"
         << "<h1>405 Method Not Allowed</h1>\n"
         << "<p>The method is not allowed for this server.</p>\n"
         << "</body>\n"
         << "</html>\n";

    response.setBody(body.str());
    return response;
}

// 生成500 Internal Server Error错误响应
// 当服务器内部发生错误时返回此响应
HttpResponse HttpResponse::internalError() {
    HttpResponse response;
    response.setStatusCode(500);
    response.setContentType("text/html");

    // 生成简单的HTML错误页面
    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>500 Internal Server Error</title></head>\n"
         << "<body>\n"
         << "<h1>500 Internal Server Error</h1>\n"
         << "<p>The server encountered an internal error.</p>\n"
         << "</body>\n"
         << "</html>\n";

    response.setBody(body.str());
    return response;
}
