#pragma once
#include <iostream>
#include <functional>
#include <sys/epoll.h>
#include <unordered_map>
#include <string.h>
#include <assert.h>
#include "channel.hpp"
#include "logger.hpp"
#define MAX_EPOLLEVENTS 1024

class Poller
{
private:
    int _epfd;                         // epoll描述符
    epoll_event _evs[MAX_EPOLLEVENTS]; // 最大可返回事件,输出活跃列表
    std::unordered_map<int, Channel *> _channels;

private: // 私有成员函数
    void update_(Channel *channel, int op)
    {
        // int epoll_ctl(int epfd, int op,  int fd,  struct epoll_event *ev);
        // EPOLL_CTL_ADD：如果该文件描述符已经存在，则操作失败。
        // EPOLL_CTL_DEL：从 epoll 实例中删除一个监听文件描述符（fd）。如果该文件描述符不存在，则操作失败。
        // EPOLL_CTL_MOD 但在实践中，通常的做法是先删除（EPOLL_CTL_DEL）再添加（EPOLL_CTL_ADD）
        // epoll_event *ev 只在 EPOLL_CTL_ADD 和 EPOLL_CTL_MOD 操作中需要
        int fd = channel->fd_();
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->get_events();
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret < 0)
            ELOG("EPOLLCTL FAILED");
        return;
    }
    bool has_channel(Channel *channel)
    {
        auto it = _channels.find(channel->fd_());
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS); // 只要大于0即可
        if (_epfd < 0)
        {
            ELOG("EPOLL CREATE FAILED");
            abort();
        }
    }
    // 添加/修改事件
    void update_event(Channel *channel)
    {
        bool ret = has_channel(channel);
        if (ret == false)
        {
            _channels.insert(std::make_pair(channel->fd_(), channel));
            return update_(channel, EPOLL_CTL_ADD);
        }
        return update_(channel, EPOLL_CTL_MOD);
    }
    // 先从map删除,再从epoll解除监控
    void remove_event(Channel *channel)
    {
        auto it = _channels.find(channel->fd_());
        if (it != _channels.end())
        {
            _channels.erase(it);
        }
        update_(channel, EPOLL_CTL_DEL);
    }
    // 开始监控,返回活跃链接,输出参数保存活跃的链接
    void poll_(std::vector<Channel *> *active)
    {
        // int epoll_wait(int epfd, struct epoll_event *evs, int maxevents, int timeout)
        // struct epoll_event *evs 输出参数
        // timeout >0等待,=0立刻返回 -1阻塞等待
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
                return;
            ELOG("EPOLL WAIT ERROR:%s\n", strerror(errno));
            abort();
        }
        // 返回的连接遍历添加到,输出参数active
        for (int i = 0; i < nfds; i++)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            it->second->set_revents(_evs[i].events); // 设置实际就绪的事件
            active->push_back(it->second);
        }
        return;
    }
};