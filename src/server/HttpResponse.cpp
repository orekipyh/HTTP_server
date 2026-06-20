#include "HttpResponse.h"
#include <sstream>
#include <algorithm>

// 构造函数：初始化状态码和状态文本映射
HttpResponse::HttpResponse() : status_code_(200) {
    // 初始化常见HTTP状态码及其描述
    status_text_[200] = "OK";
    status_text_[404] = "Not Found";
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

    // 生成Connection头（使用close关闭连接，简单处理）
    // 告诉客户端响应完成后关闭连接，下次再请求时需要重新建立TCP连接
    // 简单场景使用
    oss << "Connection: close\r\n";

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
