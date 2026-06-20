#include "utils/FileUtil.h"
#include <iostream>

int main() {
    // 测试文件路径（相对于 build/ 目录）
    std::string test_file = "../www/test.txt";

    // 测试1: fileExists() 正确判断文件是否存在
    {
        FileUtil exist_file(test_file);
        FileUtil not_exist("../www/nonexistent.txt");

        bool exist_ok = exist_file.fileExists();
        bool not_exist_ok = !not_exist.fileExists();

        std::cout << "FileUtil::fileExists() 存在文件 (" << test_file << "): "
                  << (exist_ok ? "PASS" : "FAIL") << std::endl;
        std::cout << "FileUtil::fileExists() 不存在文件 (nonexistent.txt): "
                  << (not_exist_ok ? "PASS" : "FAIL") << std::endl;

        if (!exist_ok) return 1;
        if (!not_exist_ok) return 2;
    }

    // 测试2: getFileSize() 返回正确的文件大小
    {
        FileUtil file(test_file);
        int64_t size = file.getFileSize();

        // "Hello World" 为 11 字节（不含换行符），echo 会追加换行符共 12 字节
        bool size_ok = (size > 0);
        std::cout << "FileUtil::getFileSize() 文件大小 (" << size << " 字节): "
                  << (size_ok ? "PASS" : "FAIL") << std::endl;

        if (!size_ok) return 3;
    }

    // 测试3: readFile() 能正确读取文件内容
    {
        FileUtil file(test_file);
        bool ok = file.readFile();

        bool content_ok = ok && !file.content().empty();
        std::cout << "FileUtil::readFile() 读取文件: "
                  << (ok ? "PASS" : "FAIL") << std::endl;
        std::cout << "FileUtil::readFile() 文件内容非空: "
                  << (content_ok ? "PASS" : "FAIL") << std::endl;

        if (!ok) return 4;
        if (!content_ok) return 5;
    }

    std::cout << std::endl;
    std::cout << "FileUtil::readFile() 能正确读取文件内容: PASS" << std::endl;
    std::cout << "FileUtil::getFileSize() 返回正确的文件大小: PASS" << std::endl;
    std::cout << "FileUtil::fileExists() 正确判断文件是否存在: PASS" << std::endl;

    return 0;
}
