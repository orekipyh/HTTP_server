#include "server/HttpResponse.h"
#include <iostream>

int main() {
    // 测试1: toString() 生成的响应格式正确
    {
        HttpResponse resp;
        resp.setStatusCode(200);
        resp.setContentType("text/html");
        resp.setBody("<html>Hello</html>");

        std::string response = resp.toString();

        bool has_status = (response.find("HTTP/1.1 200 OK") != std::string::npos);
        bool has_content_type = (response.find("Content-Type: text/html") != std::string::npos);
        bool has_content_length = (response.find("Content-Length:") != std::string::npos);
        bool has_body = (response.find("<html>Hello</html>") != std::string::npos);

        std::cout << "HttpResponse::toString() 格式正确 (200 OK): "
                  << (has_status ? "PASS" : "FAIL") << std::endl;

        if (!has_status) return 1;
        if (!has_content_type) return 2;
        if (!has_content_length) return 3;
        if (!has_body) return 4;
    }

    // 测试2: setContentTypeByPath() 根据文件扩展名正确映射
    {
        HttpResponse resp;

        bool html_ok = resp.setContentTypeByPath("index.html");
        bool html_type = (resp.contentType() == "text/html");

        bool css_ok = resp.setContentTypeByPath("style.css");
        bool css_type = (resp.contentType() == "text/css");

        bool js_ok = resp.setContentTypeByPath("app.js");
        bool js_type = (resp.contentType() == "application/javascript");

        bool png_ok = resp.setContentTypeByPath("image.png");
        bool png_type = (resp.contentType() == "image/png");

        bool jpg_ok = resp.setContentTypeByPath("photo.jpg");
        bool jpg_type = (resp.contentType() == "image/jpeg");

        bool all_ok = html_ok && css_ok && js_ok && png_ok && jpg_ok &&
                      html_type && css_type && js_type && png_type && jpg_type;

        std::cout << "setContentTypeByPath() 文件扩展名映射 (html/css/js/png/jpg): "
                  << (all_ok ? "PASS" : "FAIL") << std::endl;

        if (!all_ok) return 5;
    }

    // 测试3: notFound() 生成正确的 404 错误响应
    {
        HttpResponse resp = HttpResponse::notFound();
        std::string response = resp.toString();

        bool status_ok = (resp.statusCode() == 404);
        bool has_404 = (response.find("404 Not Found") != std::string::npos);
        bool type_ok = (resp.contentType() == "text/html");

        std::cout << "HttpResponse::notFound() 404 错误响应: "
                  << ((status_ok && has_404 && type_ok) ? "PASS" : "FAIL") << std::endl;

        if (!status_ok) return 6;
        if (!has_404) return 7;
        if (!type_ok) return 8;
    }

    // 测试4: internalError() 生成正确的 500 错误响应
    {
        HttpResponse resp = HttpResponse::internalError();
        std::string response = resp.toString();

        bool status_ok = (resp.statusCode() == 500);
        bool has_500 = (response.find("500 Internal Server Error") != std::string::npos);
        bool type_ok = (resp.contentType() == "text/html");

        std::cout << "HttpResponse::internalError() 500 错误响应: "
                  << ((status_ok && has_500 && type_ok) ? "PASS" : "FAIL") << std::endl;

        if (!status_ok) return 9;
        if (!has_500) return 10;
        if (!type_ok) return 11;
    }

    std::cout << std::endl;
    std::cout << "HttpResponse::toString() 生成的响应格式正确: PASS" << std::endl;
    std::cout << "Content-Type 根据文件扩展名正确映射: PASS" << std::endl;
    std::cout << "notFound() 和 internalError() 能生成正确的错误响应: PASS" << std::endl;

    return 0;
}
