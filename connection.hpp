#pragma once
#include <iostream>
#include <memory>
#include "buffer.hpp"
#include "socket.hpp"
#include "eventloop.hpp"
class Any
{
private:
    class holder
    {
    public:
        virtual ~holder() {}
        virtual const std::type_info &type() = 0;
        virtual holder *clone() = 0;
    };
    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T &val) : _val(val) {}
        // 获取子类对象保存的数据类型
        virtual const std::type_info &type() { return typeid(T); }
        // 针对当前的对象自身，克隆出一个新的子类对象
        virtual holder *clone() { return new placeholder(_val); }

    public:
        T _val;
    };
    holder *_content;

public:
    Any() : _content(NULL) {}
    template <class T>
    Any(const T &val) : _content(new placeholder<T>(val)) {}
    Any(const Any &other) : _content(other._content ? other._content->clone() : NULL) {}
    ~Any() { delete _content; }

    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    // 返回子类对象保存的数据的指针
    template <class T>
    T *get()
    {
        // 想要获取的数据类型，必须和保存的数据类型一致
        assert(typeid(T) == _content->type());
        return &((placeholder<T> *)_content)->_val;
    }
    // 赋值运算符的重载函数
    template <class T>
    Any &operator=(const T &val)
    {
        // 为val构造一个临时的通用容器，然后与当前容器自身进行指针交换，临时对象释放的时候，原先保存的数据也就被释放
        Any(val).swap(*this);
        return *this;
    }
    Any &operator=(const Any &other)
    {
        Any(other).swap(*this);
        return *this;
    }
};
// DISCONECTED -- 连接关闭状态；   CONNECTING -- 连接建立成功-待处理状态
// CONNECTED -- 连接建立完成，各种设置已完成，可以通信的状态；  DISCONNECTING -- 待关闭状态
typedef enum
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
} ConnStatu;
class Connection;
  using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection> // 便于获取share指针
{
private:
    uint64_t _conn_id;
    int _sockfd;
    bool _enable_inactive_release;
    EventLoop *_loop;   // 连接所关联的一个EventLoop
    ConnStatu _statu;   // 连接状态
    Socket _socket;     // 套接字操作管理
    Channel _channel;   // 连接的事件管理
    Buffer _in_buffer;  // 输入缓冲区---存放从socket中读取到的数据
    Buffer _out_buffer; // 输出缓冲区---存放要发送给对端的数据
    // any类型
    Any _context; // 请求的接收处理上下文

    // 由服务器来设置回调函数
    using connected_callback = std::function<void(const PtrConnection &)>;
    using message_callback = std::function<void(const PtrConnection &, Buffer *)>;
    using closed_callback = std::function<void(const PtrConnection &)>;
    using any_event_callback = std::function<void(const PtrConnection &)>;
    connected_callback _connected_callback;
    message_callback _message_callback;
    closed_callback _closed_callback;
    any_event_callback _event_callback;
    /*组件内的连接关闭回调--组件内设置的，因为服务器组件内会把所有的连接管理起来，一旦某个连接要关闭*/
    /*就应该从管理的地方移除掉自己的信息,从服务器删除*/
    closed_callback _server_closed_callback;

private:
    // 创建回调函数  放入缓冲区后处理消息回调 
    void handle_read()
    {
        // 1. 接收socket的数据，放到缓冲区
        char buf[65536];
        ssize_t ret = _socket.non_block_recv(buf, 65535);
        if (ret < 0)
        {
            // 出错了,不能直接关闭连接
            return shutdown_inloop();
        }
        // 这里的等于0表示的是没有读取到数据，而并不是连接断开了，连接断开返回的是-1
        // 将数据放入输入缓冲区,写入之后顺便将写偏移向后移动
        _in_buffer.write_normal_push(buf, ret);
        // 2. 调用message_callback进行业务处理
        if (_in_buffer.readable_size() > 0)
        { // 读回调调用消息回调,也就是业务处理函数
            // shared_from_this--从当前对象自身获取自身的shared_ptr管理对象
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }

    // 写回调 把outbuffer数据发送出去即可
    void handle_write()
    {
        //_out_buffer中保存的数据就是要发送的数据
        ssize_t ret = _socket.non_block_send(_out_buffer.read_position(), _out_buffer.readable_size());
        if (ret < 0)
        {
            // 发送错误就该关闭连接了，
            if (_in_buffer.readable_size() > 0)
            {
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return release_(); // 这时候就是实际的关闭释放操作了。
        }
        _out_buffer.move_read_offset(ret); // 千万不要忘了，将读偏移向后移动
        if (_out_buffer.readable_size() == 0)
        {
            _channel.disable_write(); // 没有数据待发送了，关闭写事件监控
            // 如果当前是连接待关闭状态，则有数据，发送完数据释放连接，没有数据则直接释放
            if (_statu == DISCONNECTING)
            {
                return release_();
            }
        }
        return;
    }
    void handle_close()
    {
        /*一旦连接挂断了，套接字就什么都干不了了，因此有数据待处理就处理一下，完毕关闭连接*/
        if (_in_buffer.readable_size() > 0)
        {
            _message_callback(shared_from_this(), &_in_buffer);
        }
        return release_();
    }
    void handle_error()
    {
        return handle_close();
    }
    // 描述符触发任意事件: 1. 刷新连接的活跃度--延迟定时销毁任务；  2. 任意事件回调
    void handle_event()
    {
        if (_enable_inactive_release == true)
        {
            _loop->timer_refresh(_conn_id);
        }
        if (_event_callback)
        {
            _event_callback(shared_from_this());
        }
    }

    // 连接获取之后，所处的状态下要进行各种设置（启动读监控,调用回调函数）
    // 这里调用相对于连接的回调函数,和事件回调有区别
    void establish_inloop()
    {
        // 1. 修改连接状态；  2. 启动读事件监控；  3. 调用回调函数
        assert(_statu == CONNECTING); // 当前的状态必须一定是上层的半连接状态
        _statu = CONNECTED;           // 当前函数执行完毕，则连接进入已完成连接状态
        // 一旦启动读事件监控就有可能会立即触发读事件，如果这时候启动了非活跃连接销毁
        _channel.enable_read();
        if (_connected_callback)
            _connected_callback(shared_from_this());
    }
    void release_inloop() // 关闭连接,当然也是要入任务队列
    {
        // 1. 修改连接状态，将其置为DISCONNECTED
        _statu = DISCONNECTED;
        // 2. 移除连接的事件监控
        _channel.remove_();
        // 3. 关闭描述符
        _socket.close_();
        // 4. 如果当前定时器队列中还有定时销毁任务，则取消任务
        if (_loop->has_timer(_conn_id))
            cancel_inactive_release(); // 在别人loop里执行线程一定正确所以都可以
        // 5. 调用关闭回调函数，避免先移除服务器管理的连接信息导致Connection被释放，再去处理会出错，因此先调用用户的回调函数
        if (_closed_callback)
            _closed_callback(shared_from_this());
        // 移除服务器内部管理的连接信息
        if (_server_closed_callback)
            _server_closed_callback(shared_from_this());
    }
    void send_inloop(Buffer &buf) // 只是将数据送到缓冲区,而且也要入队
    {
        if (_statu == DISCONNECTED)
            return;
        _out_buffer.write_buffer_push(buf);
        if (_channel.writeable() == false)
        {
            _channel.enable_write();
        }
    }
    // 这个关闭操作并非实际的连接释放操作，需要判断还有没有数据待处理，待发送
    void shutdown_inloop()
    {
        _statu = DISCONNECTING; // 设置连接为半关闭状态
        if (_in_buffer.readable_size() > 0)
        {
            if (_message_callback)
                _message_callback(shared_from_this(), &_in_buffer);
        }
        // 要么就是写入数据的时候出错关闭，要么就是没有待发送数据，直接关闭
        if (_out_buffer.readable_size() > 0)
        {
            if (_channel.writeable() == false)
            {
                _channel.enable_write();
            }
        }
        if (_out_buffer.readable_size() == 0)
        {
            release_();
        }
    }

    // 启动非活跃连接超时释放规则
    void enable_inactive_release_inloop(int sec)
    {
        // 1. 将判断标志 _enable_inactive_release 置为true
        _enable_inactive_release = true;
        // 2. 如果当前定时销毁任务已经存在，那就刷新延迟一下即可
        if (_loop->has_timer(_conn_id))
        {
            return _loop->timer_refresh(_conn_id);
        }
        // 3. 如果不存在定时销毁任务，则新增
        _loop->timer_add(_conn_id, sec, std::bind(&Connection::release_, this));
    }
    void cancel_inactive_release_inloop()
    {
        _enable_inactive_release = false;
        if (_loop->has_timer(_conn_id))
        {
            _loop->timer_cancel(_conn_id);
        }
    }
    void upgrade_inloop(const Any &context, // 更新上下文
                        const connected_callback &conn,
                        const message_callback &msg,
                        const closed_callback &closed,
                        const any_event_callback &event)
    {
        _context = context;
        _connected_callback = conn;
        _message_callback = msg;
        _closed_callback = closed;
        _event_callback = event;
    }

public:
    Connection(EventLoop *loop, uint64_t conn_id, int sockfd) : _conn_id(conn_id), _sockfd(sockfd),
                                                                _enable_inactive_release(false), _loop(loop), _statu(CONNECTING), _socket(_sockfd),
                                                                _channel(loop, _sockfd)
    {
        _channel.set_close_callback(std::bind(&Connection::handle_close, this));
        _channel.set_event_callback(std::bind(&Connection::handle_event, this));
        _channel.set_read_callback(std::bind(&Connection::handle_read, this));
        _channel.set_write_callback(std::bind(&Connection::handle_write, this));
        _channel.set_error_callback(std::bind(&Connection::handle_error, this));
    }
    ~Connection() { DLOG("RELEASE CONNECTION:%p", this); }
    int fd_() { return _sockfd; }
    int id_() { return _conn_id; }
    bool connected() { return (_statu == CONNECTED); }
    void set_context(const Any &context) { _context = context; }
    Any *get_context() { return &_context; }
    void set_connected_callback(const connected_callback &cb) { _connected_callback = cb; }
    void set_message_callback(const message_callback &cb) { _message_callback = cb; }
    void set_closed_callback(const closed_callback &cb) { _closed_callback = cb; }
    void set_any_event_callback(const any_event_callback &cb) { _event_callback = cb; }
    void set_server_closed_callback(const closed_callback &cb) { _server_closed_callback = cb; }
    // 因为connection独立于eventloop,所以要自己实现入队列,不是在eventloop调用
    void established_()
    {
        _loop->run_inloop(std::bind(&Connection::establish_inloop, this));
    }
    // 将数据写入outbuffer,一个connection要发送就用这个端口
    void send_(const char *data, size_t len)
    {
        Buffer buf;
        buf.write_normal_push(data, len);
        _loop->run_inloop(std::bind(&Connection::send_inloop, this, std::move(buf)));
    }
    void shutdown_()
    {
        _loop->run_inloop(std::bind(&Connection::shutdown_inloop, this));
    }
    void release_()
    {
        _loop->queue_inloop(std::bind(&Connection::release_inloop, this));
    }
    void enable_inactive_release(int sec)
    {
        _loop->run_inloop(std::bind(&Connection::enable_inactive_release_inloop, this, sec));
    }
    void cancel_inactive_release()
    {
        _loop->run_inloop(std::bind(&Connection::cancel_inactive_release_inloop, this));
    }

    // 切换协议---重置上下文以及阶段性回调处理函数 -- 而是这个接口必须在EventLoop线程中立即执行
    // 防备新的事件触发后，处理的时候，切换任务还没有被执行--会导致数据使用原协议处理了。
    void upgrade(const Any &context, const connected_callback &conn, const message_callback &msg,
                 const closed_callback &closed, const any_event_callback &event)
    {
        _loop->assert_loop();
        _loop->run_inloop(std::bind(&Connection::upgrade_inloop, this, context, conn, msg, closed, event));
    }
};

class Acceptor//监听套接字类
{
private:
    Socket _socket;   // 用于创建监听套接字
    EventLoop *_loop; // 用于对监听套接字进行事件监控
    Channel _channel; // 用于对监听套接字进行事件管理

    using accept_callback = std::function<void(int)>;
    accept_callback _accept_callback;

private:
    /*监听套接字的读事件回调处理函数---获取新连接，调用_accept_callback函数进行新连接处理*/
    void handle_read()
    {
        int newfd = _socket.accept_();
        if (newfd < 0)
        {
            return;
        }
        if (_accept_callback)
            _accept_callback(newfd);
    }
    int create_server(int port)
    {
        bool ret = _socket.create_server(port);
        assert(ret == true);
        return _socket.fd();
    }

public:
    /*不能将启动读事件监控，放到构造函数中，必须在设置回调函数后，再去启动*/
    /*否则有可能造成启动监控后，立即有事件，处理的时候，回调函数还没设置：新连接得不到处理，且资源泄漏*/
    Acceptor(EventLoop *loop, int port) : _socket(create_server(port)), _loop(loop),
                                          _channel(loop, _socket.fd())
    {
        _channel.set_read_callback(std::bind(&Acceptor::handle_read, this));//监听成功后,调用accept进行处理
    }
    void set_accept_callback(const accept_callback &cb) { _accept_callback = cb; }
    void Listen() { _channel.enable_read(); }
};
