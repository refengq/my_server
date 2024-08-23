#include "../tcpserver.hpp"

class EchoServer {
    private:
        TcpServer _server;
    private:
        void OnConnected(const PtrConnection &conn) {
            DLOG("NEW CONNECTION:%p", conn.get());
        }
        void OnClosed(const PtrConnection &conn) {
            DLOG("CLOSE CONNECTION:%p", conn.get());
        }
        void OnMessage(const PtrConnection &conn, Buffer *buf) {
            conn->send_(buf->read_position(), buf->readable_size());
            buf->move_read_offset(buf->readable_size());
            conn->shutdown_();
        }
    public:
        EchoServer(int port):_server(port) {
            _server.set_thread_count(2);
            _server.enable_inactive_release(10);
            _server.set_closed_callback(std::bind(&EchoServer::OnClosed, this, std::placeholders::_1));
            _server.set_connected_callback(std::bind(&EchoServer::OnConnected, this, std::placeholders::_1));
            _server.set_message_callback(std::bind(&EchoServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
        }
        void start_() { _server.start_(); }
};