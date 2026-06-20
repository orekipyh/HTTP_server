// FileUtil.h - 文件操作工具类头文件
#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <cstdint>
#include <string>

class FileUtil {
public:
    // 构造函数
    // file_path: 要操作的文件路径
    explicit FileUtil(const std::string& file_path);

    // 读取整个文件内容到内存
    // 返回: 读取成功返回true，失败返回false
    // 说明: 读取成功后可通过 content() 获取文件内容
    bool readFile();

    // 获取文件大小
    // 返回: 文件大小（字节），文件不存在返回-1
    int64_t getFileSize() const;

    // 判断文件是否存在
    // 返回: 存在返回true，不存在返回false
    bool fileExists() const;

    // 获取文件内容
    // 返回: 文件内容字符串
    const std::string& content() const { return content_; }

    // 获取错误信息
    // 返回: 最近一次错误的描述
    const std::string& error() const { return error_; }

private:
    std::string file_path_;    // 文件路径
    std::string content_;       // 文件内容
    std::string error_;         // 错误信息
};

#endif // FILE_UTIL_H
