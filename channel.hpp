#pragma once
#include <iostream>
#include <functional>
#include <sys/epoll.h>

class Poller;
class EventLoop;
class Channel // 事件管理
{
private:
    int _fd; // 描述符
    EventLoop *_loop;
    uint32_t _events;  // 当前监控
    uint32_t _revents; // 触发的监控
    // 下面是5个触发的回调函数
    using event_callback = std::function<void()>;
    event_callback _read_callback;
    event_callback _write_callback;
    event_callback _error_callback;
    event_callback _close_callback;
    event_callback _event_callback;

public:
    Channel(EventLoop *loop, int fd) : _fd(fd), _events(0), _revents(0), _loop(loop) {}
    int fd_() { return _fd; }
    uint32_t get_events() { return _events; }
    uint32_t set_revents(uint32_t events) { _revents = events; } // 回调设置监控触发事件
    // 真正执行时在单线程里进行执行
    void set_read_callback(const event_callback &cb) { _read_callback = cb; }
    void set_write_callback(const event_callback &cb) { _write_callback = cb; }
    void set_error_callback(const event_callback &cb) { _error_callback = cb; }
    void set_close_callback(const event_callback &cb) { _close_callback = cb; }
    void set_event_callback(const event_callback &cb) { _event_callback = cb; }
    // 对于各种事件是否设置的查询
    bool readable() { return (_events & EPOLLIN); }
    bool writeable() { return (_events & EPOLLOUT); }
    bool enable_read()
    {
        _events |= EPOLLIN;
        update_();
    }
    bool enable_write()
    {
        _events |= EPOLLOUT;
        update_();
    }
    void disable_read()
    {
        _events &= ~EPOLLIN;
        update_();
    }
    void disable_write()
    {
        _events &= ~EPOLLOUT;
        update_();
    }
    void disable_all()
    {
        _events=0;
        update_();
    }
    // 使用loop接口后面再实现,在eventloop.hpp里面进行实现
    void remove_();
    void update_();
    /*
    移除/更新 事件
    void Channel::Remove() { return _loop->RemoveEvent(this); }
    void Channel::Update() { return _loop->UpdateEvent(this); }
    */
    // 事件处理，一旦连接触发了事件，就调用这个函数，自己触发了什么事件如何处理自己决定
    // 总的处理函数,根据触发的revent选择事件处理
    void handle_event()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            /*不管任何事件，都调用的回调函数*/
            if (_read_callback) // 立马调用,不需要线程
                _read_callback();
        }
        /*有可能会释放连接的操作事件，一次只处理一个*/
        if (_revents & EPOLLOUT)
        {
            if (_write_callback)
                _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            if (_error_callback)
                _error_callback(); // 一旦出错，就会释放连接，因此要放到前边调用任意回调
        }
        else if (_revents & EPOLLHUP)
        {
            if (_close_callback)
                _close_callback();
        }
        if (_event_callback)
            _event_callback();
    }
};
