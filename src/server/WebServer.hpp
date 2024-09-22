#ifndef __WEB_SERVER__
#define __WEB_SERVER__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <unordered_map>

#include "Epoller.hpp"
#include "../ThreadPool/ThreadPool.hpp"
#include "../http/HttpConnection.hpp"

/*
    epoll：只是单纯的监听抵达的事件
    1. 把listen加入eventpoll
    2. 在while中 listedfd发生的事件意味着新连接的建立 accept得到fd并加入eventpoll
       对于非listenfd发生的事件，做业务处理
*/

class WebServer{
private:
    bool _init_socket();
    void _init_eventmode(int triggerMode);

    // accept到一个fd建立连接并加入eventpoll
    void _accept_conn();

    // 加入eventpoll中
    void _create_connection(int fd,sockaddr_in addr);
    void _close_connection(HttpConnection* conn);


    // 处理事件时直接交给线程池
    void _process_readevent(HttpConnection* conn);
    void _process_writeevent(HttpConnection* conn);

    // 读请求报文
    void _read_request(HttpConnection* conn);
    // conn构造响应报文
    void _process(HttpConnection* conn);
    // 写响应报文
    void _write_response(HttpConnection* conn);

    static const int MAX_FD = 65536;
    static int _set_fd_nonblock(int fd);

    int _listenfd;
    int _port;
    int _timeout_ms;
    bool _is_closed;
    char* _srcdir;

    uint32_t _listen_event;
    uint32_t _conn_event;

    std::unique_ptr<ThreadPool> _threadpool;
    std::unique_ptr<Epoller> _epoller;
    std::unordered_map<int,HttpConnection> _connections;
public:
    WebServer(int port,int triggerMode,int timeoutMs,int threadNum);
    ~WebServer();
    void start();
};

#endif  // __WEB_SERVER__