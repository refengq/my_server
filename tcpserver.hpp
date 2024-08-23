#pragma once
#include "eventloop.hpp"
#include "logger.hpp"
#include "loopthread.hpp"
#include "poller.hpp"
#include "socket.hpp"
#include "timewheel.hpp"
#include "connection.hpp"
#include "buffer.hpp"
#include "channel.hpp"

class TcpServer
{
private:
    uint64_t _next_id; // 自增不重复id
    int _port;
    int _timeout;
    bool _enable_inactive_release;                      // 是否启动了非活跃连接超时销毁的判断标
    EventLoop _baseloop;                                // 主线程的EventLoop对象，负责监听事件的处理
    Acceptor _acceptor;                                 // 监听套接字的管理对象
    LoopThreadPool _pool;                               // 从属EventLoop线程池
    std::unordered_map<uint64_t, PtrConnection> _conns; // 保存管理所有连接对应的shared_ptr对象

    using connected_callback = std::function<void(const PtrConnection &)>;
    using message_callback = std::function<void(const PtrConnection &, Buffer *)>;
    using closed_callback = std::function<void(const PtrConnection &)>;
    using any_event_callback = std::function<void(const PtrConnection &)>;
    using Functor = std::function<void()>;
    connected_callback _connected_callback;
    message_callback _message_callback;
    closed_callback _closed_callback;
    any_event_callback _event_callback;

private:
    void run_after_inloop(const Functor &task, int delay) // 添加定时任务
    {
        _next_id++;
        _baseloop.timer_add(_next_id, delay, task); //
    }
    void new_connection(int fd) // 监听的读回调函数,Established 添加监控,调用connect回调
    {
        _next_id++;
        PtrConnection conn(new Connection(_pool.next_loop(), _next_id, fd));
        conn->set_message_callback(_message_callback);
        conn->set_closed_callback(_closed_callback);
        conn->set_connected_callback(_connected_callback);
        conn->set_any_event_callback(_event_callback);
        conn->set_server_closed_callback(std::bind(&TcpServer::remove_connection, this, std::placeholders::_1));
        if (_enable_inactive_release)
            conn->enable_inactive_release(_timeout); // 启动非活跃超时销毁
        conn->established_();                        // 就绪初始化,启动读事件,挂载到poller
        _conns.insert(std::make_pair(_next_id, conn));
    }
    void remove_connection_inloop(const PtrConnection &conn) // 对服务器来讲,从数组移出就可以
    {
        int id = conn->id_();
        auto it = _conns.find(id);
        if (it != _conns.end())
        {
            _conns.erase(it);
        }
    }
    void remove_connection(const PtrConnection &conn)
    {
        _baseloop.run_inloop(std::bind(&TcpServer::remove_connection_inloop, this, conn));
    }

public:
    TcpServer(int port) : _port(port),
                          _next_id(0),
                          _enable_inactive_release(false),
                          _acceptor(&_baseloop, port),
                          _pool(&_baseloop) // 线程池也要设置baseloop
    {
        _acceptor.set_accept_callback(std::bind(&TcpServer::new_connection, this, std::placeholders::_1));
        _acceptor.Listen(); //  void Listen() { _channel.enable_read(); } 挂载到baseloop
    }

    void set_thread_count(int count) { return _pool.set_thread_count(count); }
    void set_connected_callback(const connected_callback &cb) { _connected_callback = cb; }
    void set_message_callback(const message_callback &cb) { _message_callback = cb; }
    void set_closed_callback(const closed_callback &cb) { _closed_callback = cb; }
    void set_any_event_callback(const any_event_callback &cb) { _event_callback = cb; }
    void enable_inactive_release(int timeout) // 设置定时时长, 启动定时销毁

    {
        _timeout = timeout;
        _enable_inactive_release = true;
    }
    // 用于添加一个定时任务
    void runafter_(const Functor &task, int delay)
    {
        _baseloop.run_inloop(std::bind(&TcpServer::run_after_inloop, this, task, delay));
    }
    void start_()
    {
        _pool.create_();    // 线程池启动
        _baseloop.start_(); // 监听启动
    }
};

class NetWork // 忽略向关闭的描述符写入的错误
{
public:
    NetWork()
    {
        DLOG("SIGPIPE INIT");
        signal(SIGPIPE, SIG_IGN);
    }
};
static NetWork nw;