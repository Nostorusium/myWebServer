#ifndef __HTTP_CONN__
#define __HTTP_CONN__

#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <stdlib.h>

#include "../buffer/Buffer.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

/*
HTTP连接所做的抽象：
1. 在服务器一侧，认为已经建立了与客户端的连接
   使用具体的fd与address构造
2. 能够接受来自客户端的请求报文
3. 解析客户端的请求报文
4. 构造响应报文 发送相关文件
5. 向客户端发送响应报文
HttpConnection已经完全具备收发的功能
*/

class HttpConnection{
private:
    int _fd;
    sockaddr_in _address;
    bool _is_closed;

    // iov[1]保管响应报文不包含body的部分 指向写缓冲区
    // iov[2]保管body部分 指向已载入内存的文件
    // count为用了几个iov
    int _iov_count;
    iovec _iov[2];
    
    Buffer _readbuffer;
    Buffer _writebuffer;

    HttpRequest _request;
    HttpResponse _response;
public:
    HttpConnection();
    ~HttpConnection();

    /*
        1. 先read把请求报文载入读缓冲区
        2. process解析并构造请求报文载入写缓冲器
        3. 最后write把响应报文写入fd
    */

    // 初始化连接
    void init(int sockfd,sockaddr_in addr);

    // 读请求报文 写响应报文
    ssize_t read(int* saveerrno);
    ssize_t write(int *saveerrno);

    void close_conn();
    int getfd() const;
    int getpost() const;
    const char* getip() const;
    sockaddr_in getaddr() const;

    // 根据请求构造响应
    bool process();

    bool is_keepalive() const{
        return _request.is_keepalive();
    }

    // 总共需要写多少字节
    int to_write_bytes(){
        return _iov[0].iov_len+_iov[1].iov_len;
    }

    static bool is_ET;
    static const char* srcdir;
};

#endif // __HTTP_CONN__