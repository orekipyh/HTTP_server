#include <cstddef>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>

#include "FileUtil.h"

// 构造函数：初始化文件路径
FileUtil::FileUtil(const std::string& file_path)
    : file_path_(file_path) {
}

// 读取整个文件内容到内存(全量读取，一次性读取所有内容)
// 使用二进制模式读取文件，适合读取静态资源（HTML/CSS/JS/图片等）
// 把磁盘上的文件内容读取到内存中然后作为HTTP响应的body发送给浏览器客户端
bool FileUtil::readFile() {
    // 先清空之前的内容和错误信息
    content_.clear();
    error_.clear();

    // 检查文件是否存在
    if (!fileExists()) {
        error_ = "File does not exist: " + file_path_;
        return false;
    }

    // 获取文件大小
    int64_t file_size = getFileSize();
    if (file_size < 0) {
        error_ = "Failed to get file size: " + file_path_;
        return false;
    }

    // 分配缓冲区存储文件内容
    // 使用 RAII 管理资源，避免手动 delete
    std::vector<char> buffer(file_size + 1);

    // 以二进制模式打开文件
    // "rb": r=读取模式，b=二进制模式（避免Windows下\r\n转换问题）
    FILE* fp = fopen(file_path_.c_str(), "rb");
    if (fp == nullptr) {
        error_ = "Failed to open file: " + file_path_;
        return false;
    }

    // 读取文件内容到缓冲区
    // fread返回成功读取的元素数量，不是字节数
    size_t bytes_read = fread(buffer.data(), 1, file_size, fp);

    // 确保缓冲区以null结尾（便于当作C字符串使用）
    // 加'\0'是为了拼成字符串
    buffer[bytes_read] = '\0';

    // 关闭文件
    fclose(fp);

    // 检查实际读取的字节数是否与文件大小一致
    if (static_cast<int64_t>(bytes_read) != file_size) {
        error_ = "Failed to read complete file: " + file_path_;
        return false;
    }

    // 将缓冲区内容转移到content_成员变量中
    content_ = buffer.data();

    return true;
}

// 获取文件大小
// 使用stat系统调用获取文件元数据
int64_t FileUtil::getFileSize() const {
    // struct stat: 用于存储文件元数据的结构体
    // 包括文件大小、创建时间、修改时间、权限等信息
    struct stat st;

    // stat()获取文件信息，第二个参数是输出参数
    // 成功返回0，失败返回-1
    if (stat(file_path_.c_str(), &st) != 0) {
        return -1;
    }

    // st_size是文件大小（字节）
    return st.st_size;
}

// 判断文件是否存在
// 使用access系统调用检查文件权限
bool FileUtil::fileExists() const {
    // F_OK: 检查文件是否存在
    // R_OK: 检查文件是否可读
    // 这里只检查存在性和可读性
    // access()返回0表示有权限，-1表示无权限或文件不存在
    return access(file_path_.c_str(), F_OK | R_OK) == 0;
}
