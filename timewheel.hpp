#pragma once
#include <iostream>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string.h>
#include <memory>
#include <sys/timerfd.h>
#include <unistd.h>
#include "channel.hpp"
#include "logger.hpp"

using TaskFunc = std::function<void()>;    // 任务
using ReleaseFunc = std::function<void()>; // 销毁任务
class TimerTask                            // 定时任务类
{
private:
    uint64_t _id;         // 定时任务id
    uint32_t _timeout;    // 定时任务超时时间
    bool _canceled;       // false 没被取消 true 取消
    TaskFunc _task_cb;    // 需要执行的定时任务
    ReleaseFunc _release; // 删除定时任务-删除timewhell weakptr
public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb) : _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
    ~TimerTask()
    {
        if (_canceled == false) // 析构函数内执行定时任务
            _task_cb();
        _release();
    }
    void cancel_() { _canceled = true; }
    void set_release(const ReleaseFunc &cb) { _release = cb; }
    uint32_t delay_time() { return _timeout; }
};

class TimerWheel
{
private:
    using WeakTask = std::weak_ptr<TimerTask>;      // s索引
    using PtrTask = std::shared_ptr<TimerTask>;     // 时间轮
    int _tick;                                      //
    int _capacity;                                  //
    std::vector<std::vector<PtrTask>> _wheel;       // 存储共享指针
    std::unordered_map<uint64_t, WeakTask> _timers; // 索引weak指针
    EventLoop *_loop;
    int _timerfd;                            // 定时器描述符
    std::unique_ptr<Channel> _timer_channel; // 定时器channel,设置回调
public:
    void remove_timer(uint64_t id) // 组合使用?
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }
    static int create_timer_fd()
    {
        // CLOCK_MONOTONIC 是一种特殊的时钟源，它提供了单调递增的时间值
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ELOG("TIMERFD CREATE FAILED!");
            abort();
        }
        // int timerfd_settime(int fd, int flags, struct itimerspec *new, struct itimerspec *old);
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0; // 第一次超时时间为1s后
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0; // 第一次超时后，每次超时的间隔时
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }
    int read_timer_fd()
    {
        uint64_t times;
        int ret = read(_timerfd, &times, 8); // 每次只能读8个字节
        if (ret < 0)
        {
            ELOG("READ TIMEFD FAILED!");
            abort();
        }
        return times;
    }
    void run_timer_task()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空数组,析构函数自动执行
    }
    void on_time() // 就是可读事件的回调函数
    {
        // 根据实际超时的次数，执行对应的超时任务
        int times = read_timer_fd();
        for (int i = 0; i < times; i++)
        {
            run_timer_task(); // 超时几次就移动几次,用来执行任务
        }
    }
    void timer_add(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        PtrTask pt(new TimerTask(id, delay, cb));
        pt->set_release(std::bind(&TimerWheel::remove_timer, this, id));
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
        _timers[id] = WeakTask(pt);
    }

    void timer_refresh(uint64_t id)
    {
        // 通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr
        int delay = pt->delay_time();   // 获取本来定时时间
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }
    void timer_cancel(uint64_t id) // 取消定时任务
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock();
        if (pt)
            pt->cancel_(); // 改变有效指针即可
    }

public:
    TimerWheel(EventLoop *loop) : _capacity(60), _tick(0), _wheel(_capacity), _loop(loop),
                                  _timerfd(create_timer_fd()), _timer_channel(new Channel(_loop, _timerfd))
    {
        _timer_channel->set_read_callback(std::bind(&TimerWheel::on_time, this));
        _timer_channel->enable_read(); // 启动读事件监控
    }
     bool has_timer(uint64_t id) //因为返回的是bool,所以必须立马执行,不能任务池
     {
            auto it = _timers.find(id);
            if (it == _timers.end()) {
                return false;
            }
            return true;
        }
/*
//可以在eventloop里面提供,其实没区别吧,反正也是直接入队操作,先不设置
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
//刷新/延迟定时任务
void TimerWheel::TimerRefresh(uint64_t id) {
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id) {
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}
*/
};

