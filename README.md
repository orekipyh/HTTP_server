# HTTP 静态文件服务器

基于 C++11 与 Linux 平台、单 Reactor 多线程架构实现的 HTTP/1.1 静态文件服务器，采用 epoll IO 多路复用（水平触发 LT 模式）处理高并发连接。

## 技术栈

- **语言**: C++11
- **IO 多路复用**: epoll（水平触发 LT 模式）
- **并发模型**: 单 Reactor + 自研线程池（生产者-消费者模型）
- **构建**: CMake

## 功能特性

- **HTTP-GET 请求解析**：完整解析请求行、请求头
- **MIME 类型映射**：支持 html、css、js、png、jpg 等多种常见文件类型
- **静态资源文件读取**：基于根目录配置的安全文件访问
- **目录遍历防护**：请求路径合法性校验，防止路径穿越攻击
- **状态码响应**：200 OK / 404 Not Found 完整响应组装
- **高并发支持**：epoll ET 模式 + 线程池异步处理业务逻辑
- **Socket 封装**：非阻塞设置、端口复用、全链路 Socket 生命周期管理

## 快速开始

```bash
# 编译
mkdir build && cd build
cmake ..
make

# 运行
./bin/http_server <port> <root_directory>

# 示例
./bin/http_server 8080 ../www/
```

## 压测结果

使用 ApacheBench (ab) 在虚拟机（2 核 CPU / 2GB 内存）上进行压测。

| 并发数 | 总请求数 | QPS (req/s) | 失败数 | 平均延迟 (ms) | P99 延迟 (ms) |
|--------|---------|-------------|--------|--------------|--------------|
| 100 | 10000 | 480.52 | 0 | 208.11 | 11419 |

## 项目结构

```
HTTP_server/
├── src/
│   ├── main.cpp          # 入口
│   ├── server/
│   │   ├── HttpServer.h/cpp  # 服务器主类（事件循环）
│   │   ├── Socket.h/cpp      # Socket 封装
│   │   ├── Epoll.h/cpp       # epoll 多路复用封装
│   │   ├── HttpRequest.h/cpp # HTTP 请求解析
│   │   └── HttpResponse.h/cpp# HTTP 响应组装
│   ├── thread/
│   │   └── ThreadPool.h/cpp  # 线程池
│   └── utils/
│       └── FileUtil.h/cpp    # 文件操作工具
├── www/                  # 静态资源目录
├── CMakeLists.txt
└── README.md
```

## 架构设计

```
客户端连接 → epoll 监听（LT 模式）
                  ↓
            单 Reactor 线程（事件分发）
                  ↓
     ┌────────────┼────────────┐
     ↓            ↓            ↓
 线程池 worker  线程池 worker  线程池 worker
（解析请求）  （读取文件）  （构建响应）
     ↓
  epoll 写事件就绪 → 发送响应给客户端
```

## 关键实现

- **epoll LT 模式**：水平触发模式下可靠处理 IO 事件，配合非阻塞 Socket 避免线程阻塞
- **线程池异步处理**：I/O 线程仅负责事件分发，耗时业务逻辑交给线程池
- **内存管理**：使用 RAII 和智能指针管理 Socket、Epoll 等资源生命周期
- **信号处理**：处理 SIGINT 信号实现优雅关闭，避免信号中断导致事件循环提前退出
