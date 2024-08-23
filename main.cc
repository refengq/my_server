#include"tcpserver.hpp"
void bufftest()
{
    // 多打断点多调试,分布找错误
    using namespace std;
    Buffer bf;
    string s("12345678\r\n");
    for (int i = 0; i < 300; i++)
    {
        bf.write_string_push(s);
    }
    // bf.write_string_push(s);
    cout << "---------------------" << endl;
    cout << bf.write_position() - bf.read_position() << endl;
    bf.write_string_push(s);
    cout << bf.write_position() - bf.read_position() << endl;
    cout << "写入测试完毕" << endl;
    // 正确
    s = bf.read_asstring(5);
    cout << bf.read_position() - bf.buffer_begin() << endl;
    cout << s << endl;
    cout << "第一次read结束" << endl;
    s = bf.read_asstring_pop(5);
    cout << bf.read_position() - bf.buffer_begin() << endl;
    cout << s << endl;
    cout << "第二次read+pop结束" << endl;

    s = bf.read_asstring(5);
    cout << bf.read_position() - bf.buffer_begin() << endl;
    cout << s << endl;
    cout << "第三次read+pop结束" << endl;
    cout << bf.write_position() - bf.read_position() << endl;
    cout << "---------------------" << endl;

    Buffer bf1;
    cout << "创建成功" << endl;
    bf1.write_buffer_push(bf);

    cout << "first" << endl;
    s = bf1.read_asstring(5);
    cout << "second" << endl;

    cout << s << endl;
    cout << "---------------------" << endl;
    s = bf.get_line();
    cout << s << endl;
    cout << "结束" << endl;
}

void sockettest()
{
    using namespace std;
    Socket server;
    server.create_server(8080);
    Socket client;
    client.create_client(8080, "127.0.0.1");
    int newfd = server.accept_();
    Socket newser(newfd);
    string s("hello server");
    client.non_block_send((void *)s.c_str(), s.size());
    char rec[100] = {0};
    auto ret = newser.non_block_recv(rec, 100);
    cout << rec << endl;
}
//
void pollertest()//没法添加新事件,无法测试,需要eventloop
{
    using namespace std;
    Socket server;
    server.create_server(8080);
    Socket client;
    client.create_client(8080, "127.0.0.1");
    int newfd = server.accept_();
    Socket newser(newfd);
    string s("hello server"); 
    // 创建一个服务端和客户端,用poller管理
    Channel ch1(nullptr, server.fd());
    //ch1.enable_read();
    Channel ch2(nullptr, client.fd());
    Poller pl;
    pl.update_event(&ch1);
    pl.update_event(&ch2);

}
/*
EventLoop() : _thread_id(std::this_thread::get_id()),
                  _event_fd(create_eventfd()),
                  _event_channel(new Channel(this, _event_fd)),
                  _timer_wheel(this)
    {
        _event_channel->set_read_callback(std::bind(&EventLoop::read_eventfd, this));
        _event_channel->enable_read(); // 还未实现update
    }
*/
void fun()
{
    DLOG("dingshiqijieshu");
}
void eventlooptest()//测试信息,成功
{
    using namespace std;
    Socket server;
    server.create_server(8080);
    Socket client;
    client.create_client(8080, "127.0.0.1");
    int newfd = server.accept_();
    Socket newser(newfd);
    string s("hello server"); 

    EventLoop eloop;
    EventLoop* test=&eloop;
    // 创建一个服务端和客户端,用poller管理
    Channel ch1(test, server.fd());
    //ch1.enable_read();
    Channel ch2(test, client.fd());
     DLOG("time kaishi");
    eloop.timer_add(1,10,fun);//先设置再运行
    eloop.start_();//死循环啊
    // DLOG("time kaishi");
    // eloop.timer_add(1,5,fun);
}


int main()
{
    using namespace std;
    Socket client;
    client.create_client(8080, "127.0.0.1");
    string s("hello server");
    client.non_block_send((void *)s.c_str(), s.size());
    sleep(1);
    char ret[100]={0};
  client.non_block_recv((void*)ret,99);
   cout<<ret<<endl;
}