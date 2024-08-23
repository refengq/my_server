#pragma once
#include <iostream>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string.h>
#include <memory>
#include <sys/timerfd.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <sys/eventfd.h>
#include "channel.hpp" //还要实现函数
#include "logger.hpp"
#include "poller.hpp"
#include "timewheel.hpp"

// 在于线程和任务的关系
class EventLoop
{
private:
    using Functor = std::function<void()>;
    std::thread::id _thread_id;
    int _event_fd; // 唤醒IO事件监控导致的阻塞
    std::unique_ptr<Channel> _event_channel;
    Poller _poller;
    std::vector<Functor> _tasks;
    std::mutex _mutex;       // 任务池线程安全
    TimerWheel _timer_wheel; // 定时器模块
public:
    void run_all_task()
    {
        std::vector<Functor> functor; // 加锁将任务换出
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.swap(functor);
        }
        for (auto &f : functor)
        {
            f();
        }
        return;
    }

    static int create_eventfd()
    {
        // initval：这是事件计数器（event counter）的初始值
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            ELOG("CREATE EVENTFD FAILED");
            abort();
        }
        return efd;
    }
    void read_eventfd() //
    {
        uint64_t res = 0;
        // 也是一次八字节
        int ret = read(_event_fd, &res, sizeof(res));
        if (ret < 0)
        {
            // EINTR -- 被信号打断；   EAGAIN -- 表示无数据可读
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            ELOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }
    void wakeup_eventfd() // 唤醒
    {
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ELOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }

public:
    EventLoop() : _thread_id(std::this_thread::get_id()),
                  _event_fd(create_eventfd()),
                  _event_channel(new Channel(this, _event_fd)),
                  _timer_wheel(this)
    {
        _event_channel->set_read_callback(std::bind(&EventLoop::read_eventfd, this));
        _event_channel->enable_read(); // 还未实现update
    }
    void start_()
    {
        while (1)
        {
            // 事件监控  回调事件处理 执行任务
            std::vector<Channel *> actives;
            _poller.poll_(&actives);
            for (auto &channel : actives)
            {
                channel->handle_event(); // 本线程,回调处理
            }
            run_all_task();
        }
    }
    // 判断当前任务是否处于当前线程,是则执行,否则放入任务队列
    bool is_inloop()
    {
        return (_thread_id == std::this_thread::get_id());
    }
    void assert_loop()
    {
        assert(_thread_id == std::this_thread::get_id());
    }
    void run_inloop(const Functor &cb)
    {
        if (is_inloop())
            return cb();
        return queue_inloop(cb);
    }

    void queue_inloop(const Functor &cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        wakeup_eventfd(); // 触发可读事件,避免阻塞,向下运行任务池
    }

    void update_event(Channel *channel){ return _poller.update_event(channel);  }
    void remove_event(Channel*channel){return _poller.remove_event(channel);}
    //时间函数
    void timer_add(uint64_t id,uint32_t delay,const TaskFunc&cb)
    {
        //让组件只管单线程,多线程安全交给eventloop操作
        run_inloop(std::bind(&TimerWheel::timer_add, &_timer_wheel, id, delay, cb));
    }
    void timer_refresh(uint64_t id)
    {
        return run_inloop(std::bind(&TimerWheel::timer_refresh, &_timer_wheel, id));
    }
    void timer_cancel(uint64_t id)
    {
        return run_inloop(std::bind(&TimerWheel::timer_cancel, &_timer_wheel, id));
    }
    bool has_timer(uint64_t id) { return _timer_wheel.has_timer(id); }

};

inline void Channel::remove_()
{
return _loop->remove_event(this); 
}
inline void Channel::update_()
{
return _loop->update_event(this);
}