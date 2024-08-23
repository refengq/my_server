#pragma once
#include <iostream>
#include <mutex>
#include <condition_variable>
#include "eventloop.hpp"

// 为了先创建线程,再创建线程对应的eventloop对象,故封装在此类里面
class LoopThread
{
private:
    std::mutex _mutex;             // 互斥锁
    std::condition_variable _cond; // 保证初始化创建线程在获取对象之前,实现同步
    EventLoop *_loop;              // EventLoop指针变量，这个对象需要在线程内实例化
    std::thread _thread;           // EventLoop对应的线程
private:
    void thread_entry()
    {
        EventLoop loop; // 生命周期随对象
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all();
        }
        loop.start_(); // 死循环
    }

public:
    LoopThread() : _loop(NULL), _thread(std::thread(&LoopThread::thread_entry, this)) {};
    EventLoop *get_loop()
    {
        EventLoop *loop = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&]()
                       { return _loop != NULL; });
            loop = _loop;
        }
        return loop;
    }
};

class LoopThreadPool
{
    // 注意至少有一个默认线程,用来处理连接
private:
    int _thread_count;
    int _next_idx;
    EventLoop *_baseloop;
    std::vector<LoopThread *> _threads; // 线程池
    std::vector<EventLoop *> _loops;    //
public:
    LoopThreadPool(EventLoop *baseloop) : _thread_count(0), _next_idx(0), _baseloop(baseloop) {}
    void set_thread_count(int count) { _thread_count = count; }
    void create_()
    {
        if (_thread_count > 0)
        {
            _threads.resize(_thread_count);
            _loops.resize(_thread_count);
            for (int i = 0; i < _thread_count; i++)
            {
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->get_loop();
            }
        }
        return;
    }
    EventLoop *next_loop()
    {
        if (_thread_count == 0)
        {
            return _baseloop;
        }
        _next_idx = (_next_idx + 1) % _thread_count;
        return _loops[_next_idx];
    }
};