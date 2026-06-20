// Socket.h - Socket封装类头文件
#ifndef SOCKET_H
#define SOCKET_H

#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

class Socket {
public:
    Socket();
    ~Socket();
    
    bool create();
    bool setReuseAddr(bool on);
    bool setKeepAlive(bool on);
    bool setNonBlocking(bool on);
    bool bind(int port);
    bool listen(int backlog = 1024);
    int accept(struct sockaddr_in* addr = nullptr);
    void close();
    
    int getFd() const { return fd_; }
    bool isValid() const { return fd_ >= 0; }

private:
    int fd_;
};

#endif
