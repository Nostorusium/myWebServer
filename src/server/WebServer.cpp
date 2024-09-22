#include "WebServer.hpp"
/*
    Connection是客户端与服务端之间连接的抽象
    一个fd对应一个Connection 它具备由该fd读写HTTP请求/响应报文的能力
    
    1. 监听fd加入eventpoll
    2. 监听到listenfd上的event意味着有新连接加入 accept
    3. accept到的新fd将作为一个新的连接，并加入epollevent管理
    4. 非listenfd发生event，即连接中发生的读写事件
    5. 如果是读事件，意味着请求报文的送达
       如果是写事件，意味着响应报文的发送
    6. 读事件发生，connection读取请求报文 调用read载入到读缓冲区并构造响应
       写事件发生,connection构造响应报文 调用write将写缓冲区的响应发出
    7. 上述IO事件处理交给线程池
*/

// 初始化
WebServer::WebServer(int port,int triggerMode,int timeoutMs,int threadNum){
    std::cout<<"WebServer Contrust\n";
    _threadpool = std::make_unique<ThreadPool>(threadNum);
    _epoller = std::make_unique<Epoller>(1024);
    // 确定本地目录地址
    // 可自动分配
    _srcdir = getcwd(nullptr,256);
    strncat(_srcdir,"/resources",16);
    HttpConnection::srcdir = _srcdir;

    _is_closed = false;
    _timeout_ms = timeoutMs;
    _port = port;

    _init_eventmode(triggerMode);
    if(_init_socket() == false){
        std::cout<<"socket init failed\n";
        _is_closed = true;
    }
    
    _threadpool->start();
}

WebServer::~WebServer(){
    close(_listenfd);
    _is_closed = true;
    free(_srcdir);
    _threadpool->stop();
}

void WebServer::start(){
    std::cout<<"WebServer start\n";
    int time_ms = -1;
    while(!_is_closed){
        int event_count = _epoller->wait(time_ms);
        for(int i=0;i<event_count;i++){
            // 处理事件
            int current_fd = _epoller->get_eventfd(i);
            uint32_t event = _epoller->get_event(i);

            if(current_fd == _listenfd){
                _accept_conn();
            }else if(event & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                _close_connection(&_connections[current_fd]);
            }else if(event & EPOLLIN){
                _read_request(&_connections[current_fd]);
            }else if(event & EPOLLOUT){
                _write_response(&_connections[current_fd]);
            }
        }
    }// while end
}

// TODO：端口复用
bool WebServer::_init_socket(){
    std::cout<<"WebServer init socket\n";
    // init socket
    int ret;
    sockaddr_in addr;
    if(_port>65535||_port<1024){
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_port);

    // socket
    _listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(_listenfd < 0){
        return false;
    }

    // bind
    ret = bind(_listenfd,(sockaddr*)&addr,sizeof(addr));
    if(ret<0){
        close(_listenfd);
        return false;
    }

    // listen
    ret = listen(_listenfd,6);
    if(ret<0){
        close(_listenfd);
        return false;
    }
    ret = _epoller->add_fd(_listenfd,_listen_event | EPOLLIN);
    if(ret == 0){
        close(_listenfd);
        return false;
    }
    // 设置非阻塞
    _set_fd_nonblock(_listenfd);
    return true;
}

void WebServer::_init_eventmode(int triggerMode){
    // EPOLLRDHUP表示断开连接
    // 注册EPOLLONESHOT可以保证任意时刻只有一个线程处理该socket
    _listen_event = EPOLLRDHUP;
    _conn_event = EPOLLONESHOT | EPOLLRDHUP;
    switch(triggerMode){
    case 0:
        break;
    case 1:
        _conn_event |= EPOLLET;
        break;
    case 2:
        _listen_event |= EPOLLET;
        break;
    case 3:
        _listen_event |= EPOLLET;
        _conn_event |= EPOLLET;
        break;
    default:
        _listen_event |= EPOLLET;
        _conn_event |= EPOLLET;
        break;
    }
    HttpConnection::is_ET = _conn_event & EPOLLET;
}

void WebServer::_accept_conn(){
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do{
        int fd = accept(_listenfd,(sockaddr*)&addr,&len);
        if(fd<=0) return;

        _create_connection(fd,addr);

    }while(_listen_event & EPOLLET);
}

// fd建立连接 并加入epoll
void WebServer::_create_connection(int fd,sockaddr_in addr){
    _connections[fd].init(fd,addr);
    _epoller->add_fd(fd,EPOLLIN|_conn_event);
    _set_fd_nonblock(fd);
}

// 读事件处理
void WebServer::_process_readevent(HttpConnection* conn){
    
    _threadpool->add_task(std::bind(&WebServer::_read_request,this,conn));
}

// 写事件处理
void WebServer::_process_writeevent(HttpConnection* conn){
    _threadpool->add_task(std::bind(&WebServer::_write_response,this,conn));
}

// 在这里卡住了 readv 不可用
void WebServer::_read_request(HttpConnection* conn){
    std::cout<<"WebServer::read request"<<"\n";
    std::cout<<"    listenfd "<<_listenfd<<"\n";
    int ret = -1;
    int read_errno = 0;
    ret = conn->read(&read_errno);
    std::cout<<"WebServer::read request over, ret="<<ret<<"\n";

    // 如果<=0 说明本轮没读到数据
    // 比如内核读缓冲区为空 那么ret会返回-1
    // 因为是非阻塞IO，会报EAGAIN
    // 0通常代表连接已关闭 -1可能是EAGAIN

    // 如果不是EAGAIN 说明对侧的连接已经关闭了
    if(ret<=0 && read_errno!=EAGAIN){
        _close_connection(conn);
        return;
    }
    
    // 如果是EAGAIN 说明此时已经把读缓冲区读完了
    // 尝试进行下一步

    _process(conn);
}

// 构造响应报文/载入待传输的文件到内存
// 如果响应报文还没有传输完全/传输失败 那就不转入EPOLLOUT 继续维持EPOLLIN
// 这个地方应该判断一下浏览器的报文是否已经完全发送过来了
// 但Request完全没有提供这个支持
// 但考虑到请求报文一般都很小 应当是能直接发送过来的
void WebServer::_process(HttpConnection* conn){
    std::cout<<"WebServer::process check request message"<<"\n";
    int ret = conn->process();
    if(ret){
        // 如果成功，接下来就该发送了，event置为OUT
        std::cout<<"    set EPOLLOUT"<<"\n";
        _epoller->mod_fd(conn->getfd(),EPOLLOUT | _conn_event);
    }else{
        // 无请求抵达 如EAGAIN，则重置event为IN
        std::cout<<"    set EPOLLIN"<<"\n";
        _epoller->mod_fd(conn->getfd(),EPOLLIN | _conn_event);
    }
}

/*
    关于非阻塞IO与传输大文件
    因为要提供Range支持，现在服务器已经支持了根据Range发送特定部分的功能。
    浏览器拖动进度条是这样做的：
    比如一个 0~1919810大小的视频
    当你拖动进度到进度114514 那么Range范围是:114514-
    响应头会标注: 114514-1919810/1919811
    接下来服务器会传输这个范围的文件到浏览器
    然而浏览器不会一次性接受整个文件 而是在接收到了足够播放当前段的数据后就暂时停止数据接收
    这样一来到来的数据就堆积在浏览器的读缓冲区和服务器的写缓冲区
    这个时候非阻塞IO就会提示EAGAIN
    直到浏览器再次处理自己写缓冲区里的数据 因为进度前移了需要播放新的内容
    我方服务器的写缓冲区再次有空余 此时在进行写入

    不管是LT和ET模式都会按照这个模式发生EAGAIN
    ET和LT的主要区别在于发送数据的主动权
    LT发送数据的行为由事件触发 每次触发就发一次 不保证发完
    ET发送数据的行为由自己的大循环搞定 每次触发就while循环尽最大努力发送 保证发完或者被阻塞/EAGAIN
*/

// 写响应报文
void WebServer::_write_response(HttpConnection* conn){
    std::cout<<"WebServer::write response"<<"\n";
    int len = -1;
    int write_errno = 0;
    std::cout<<"    write conn"<<"\n";
    len = conn->write(&write_errno);
    std::cout<<"    write len this time: "<<len<<"\n";
    // 若传输完成
    if(conn->to_write_bytes() == 0){
        // 持久化
        if(conn->is_keepalive()){
            // 进入第二分支等待写入
            _process(conn);
            return;
        }
    }else if(len<0){
        // 如果是因为非阻塞IO未完成造成的EAGAIN
        // 比如你把数据传过去了结果浏览器不想要
        if(write_errno == EAGAIN){
            std::cout<<"    wrtie_error: EAGAIN!\n";
            std::cout<<"    mod_fd: EPOLLOUT\n";
            // 重置该fd监听的事件 继续传输
            // 边缘触发在没有读完数据时重置事件，可以再次触发
            // 水平触发没区别
            _epoller->mod_fd(conn->getfd(),_conn_event|EPOLLOUT);
            return;
        }
    }

    // 非持久化
    _close_connection(conn);
}

void WebServer::_close_connection(HttpConnection* conn){
    _epoller->del_fd(conn->getfd());
    conn->close_conn();
}

int WebServer::_set_fd_nonblock(int fd){
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}