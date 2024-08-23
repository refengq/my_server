#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include "logger.hpp"
#include <fcntl.h>

#define MAX_LISTEN 1024
class Socket
{
private:
    int _sockfd;

public:
    Socket() : _sockfd(-1) {}
    Socket(int fd) : _sockfd(fd) {}
    ~Socket() { close_(); }
    int fd() { return _sockfd; }
    bool create_()
    {
        // int socket(int domain, int type, int protocol)
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd < 0)
        {
            ELOG("CREATE SOCKET FAILED ");
            return false;
        }
        return true;
    }
    bool bind_(const std::string &ip, uint16_t port)
    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        int len = sizeof(sockaddr_in);
        int ret = bind(_sockfd, (sockaddr *)&addr, len);
        if (ret < 0)
        {
            ELOG("BIND ADDRESS FAILED ");
            return false;
        }
        return true;
    }
    bool listen_(int backlog = MAX_LISTEN)
    {
        int ret = listen(_sockfd, backlog);
        if (ret < 0)
        {
            ELOG("SOCKET LISTEN FAILED");
            return false;
        }
        return true;
    }
    bool connect_(const std::string &ip, uint16_t port) // 向服务端发起连接
    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        int len = sizeof(sockaddr_in);
        int ret = connect(_sockfd, (sockaddr *)&addr, len);
        if (ret < 0)
        {
            ELOG("CONNECT SERVER FAILED ");
            return false;
        }
        return true;
    }
    int accept_()
    {
           // int accept(int sockfd, struct sockaddr *addr, socklen_t *len);
            int newfd = accept(_sockfd, NULL, NULL);
            if (newfd < 0) {
                ELOG("SOCKET ACCEPT FAILED!");
                return -1;
            }
            return newfd;
    }
    // typedef long ssize_t
    ssize_t recv_(void *buf, size_t len, int flag = 0)
    {
        // sockfd 是非阻塞的，你不需要设置 MSG_DONTWAIT，因为套接字已经是非阻塞模式，send 会自动按照非阻塞模式操作。
        ssize_t ret = recv(_sockfd, buf, len, flag); // 阻塞+数据移出
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0; // 特殊中断,可以接受
            ELOG("SOCKET RECV FAILED");
            return -1;
        }
        return ret;
    }
    ssize_t non_block_recv(void *buf, size_t len)
    {
        return recv_(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞。
    }
    ssize_t send_(const void *buf, size_t len, int flag = 0)
    {
        ssize_t ret = send(_sockfd, buf, len, flag); // 阻塞+数据移出
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0; // 特殊中断,可以接受
            ELOG("SOCKET SEND FAILED");
            return -1;
        }
        return ret;
    }
    ssize_t non_block_send(void *buf, size_t len)
    {
        if (len == 0)
            return 0;
        return send_(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞。
    }
    void close_()
    {
        if (_sockfd != -1)
            close(_sockfd);
        _sockfd = -1;
    }
    // 集成功能,创建服务端连接 默认非阻塞
    bool create_server(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        // 创建 非阻塞 绑定 监听 启用地址重用
        if (create_() == false)
            return false;
        if (block_flag)
            non_block();
        if (bind_(ip, port) == false)
            return false;
        if (listen_() == false)
            return false;
        reuse_address();
        return true;
    }
    // 创建客户端连接
    bool create_client(uint16_t port, const std::string &ip)
    {
        // 创建 连接服务器
        if (create_() == false)
            return false;
        if (connect_(ip, port) == false)
            return false;
        return true;
    }
    void reuse_address()
    {
        // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
        int val = 1; // 地址重用
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        val = 1; // 端口重用
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }
    void non_block()
    {
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
};