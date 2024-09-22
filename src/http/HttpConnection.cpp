#include "HttpConnection.hpp"
#include <iostream>

using namespace std;

const char* HttpConnection::srcdir;
bool HttpConnection::is_ET = false;

HttpConnection::HttpConnection(){
    _fd = -1;
    _address = {0};
    _is_closed = true;
}

HttpConnection::~HttpConnection(){
    close_conn();
}

void HttpConnection::init(int sockfd,sockaddr_in addr){
    if(sockfd<=0){
        perror("HttpConnection fd <= 0");
    }

    _address = addr;
    _fd = sockfd;
    _writebuffer.retrieve_all();
    _readbuffer.retrieve_all();
    _is_closed = false;
}

void HttpConnection::close_conn(){
    _response.unmap_file();
    if(_is_closed == false){
        _is_closed = true;
        close(_fd);
    }
}

int HttpConnection::getfd() const{
    return _fd;
}

sockaddr_in HttpConnection::getaddr() const{
    return _address;
}

const char* HttpConnection::getip() const{
    return inet_ntoa(_address.sin_addr);
}

int HttpConnection::getpost() const{
    return _address.sin_port;
}

// 读请求报文到读缓冲区
ssize_t HttpConnection::read(int* saveerrno){
    std::cout<<"HttpConnection::read"<<"\n";
    ssize_t len = -1;
    // 如果是边缘模式就循环
    // 如果是水平模式就读一次
    do{
        len = _readbuffer.read_from_fd(_fd,saveerrno);
        std::cout<<"HttpConnection::read buffer.readfrom_fd()\n";
        std::cout<<"HttpConnection::read _fd:"<<_fd<<"\n";
        if(len<=0) break;
    }while(is_ET);
    // 如果是ET模式 len最终会<=0
    return len;
}

/*
    _readbuffer: 请求报文会写入该位置
    _writebuffer: 用于发送响应和文件
    构造响应报文 准备write发送
*/
bool HttpConnection::process(){
    std::cout<<"HttpConnection::process"<<"\n";

    // check request

    _request.init();
    // 对侧写入读缓冲区 如果无请求抵达 直接返回false
    if(_readbuffer.get_readable_bytes()<=0){
        return false;
    }

    int ret = _request.parse(_readbuffer);
    // 解析请求报文 构造响应报文
    if(ret){
        _response.init(srcdir,_request.path(),_request.is_keepalive(),200);
    }else{
        _response.init(srcdir,_request.path(),false,400);
    }

    std::cout<<_request.path()<<"\n";

    // 向写缓存写入响应报文(不含文件)
    // 在载入文件到内存之前 先判断一下mmfile
    // 先释放旧的mmfile再弄申请新的
    // 防止爆内存
    if(_response._mmfile!=nullptr){
        _response.unmap_file();
    }

    std::string start_str = "";
    std::string end_str = "";
    bool is_range = false;

    // 查看请求报文是否需要Range
    // 注意Range里的值都是偏移量
    if(_request._headers.count("Range") == 1){
        is_range = true;
        std::string range_header = _request._headers["Range"];
        std::cout<<"    range_header:"<<range_header<<"\n";
        std::string prefix = "bytes=";
        if(!range_header.empty()){
            //不指定size则到末尾
            // range_str: xxxx-xxxx
            std::string range_str = range_header.substr(prefix.size());
            // 响应报文加上range的响应header
            // find返回index 而不是偏移量
            size_t pos_divide = range_str.find('-');

            if(pos_divide != std::string::npos){
                start_str = range_str.substr(0,pos_divide);
                if(pos_divide+1<range_str.size()){
                    end_str =range_str.substr(pos_divide+1);
                }
            }
        }
    }

    // is_range set code: 206
    _response.check_code(_writebuffer,is_range);
    // 如果code为错误页面
    _response._checkcode_errorhtml(); // reset filepath and htmlpage if error
    _response._add_stateline(_writebuffer);
    _response._add_basic_header(_writebuffer);
    bool load_success = _response._load_content(_writebuffer);
    // 没用上↑

    // 成功把文件载入内存
    // 接下来处理206 range逻辑
    size_t start = 0;
    size_t end = _response.file_length()-1;

    if(start_str != ""){
        start = std::stoi(start_str);
    }
    if(end_str != ""){
        end = std::stoi(end_str);
        std::cout<<"    end_str turn into int:" <<end <<"\n";
    }

    _response._add_range_header(_writebuffer,start,end);
    _response._add_contentlen_header(_writebuffer,end-start+1);
    _response._add_endline(_writebuffer);

    // 响应报文不包括body的部分
    _iov[0].iov_base = const_cast<char*>(_writebuffer.peek());
    _iov[0].iov_len = _writebuffer.get_readable_bytes();
    _iov_count = 1;

    // 不知为何这里不支持偏移 再试试
    // 好好好 可以了
    // 如果_response.file没成功载入那就不执行这些
    // 因为errorhtml直接写入_writebuffer了
    if(_response.file_length()>0 && _response.file()){
        _iov[1].iov_base = _response.file()+start;
        //_iov[1].iov_len = _response.file_length();
        _iov[1].iov_len = end-start+1;
        _iov_count = 2;
    }

    return true;
}

// 写缓冲区对侧响应报文写入fd
// 对比一下水平触发和边缘触发
// 水平触发不要求一次性处理好缓冲区中的所有内容
// 边缘触发则需要while一次性全部处理掉
ssize_t HttpConnection::write(int *saveerrno){
    std::cout<<"HttpConnection::write,write response and file to client"<<"\n";
    ssize_t len = -1;
    std::cout<<"    check response: "<<(char*)_iov[0].iov_base<<"\n";
    do{
        std::cout<<"    writev loop"<<"\n";
        // 似乎没有处理好-1的问题 阻塞在了某个地方
        len = writev(_fd,_iov,_iov_count);
        std::cout<<"    written this time: "<<len<<"\n";
        std::cout<<"    Left: "<<this->to_write_bytes()<<"\n";

        // 非阻塞IO情况下可能会发生EAGAIN
        // 表示没有数据可读，稍后再试
        // 经常可能报错Resource temporarily unavailable
        // 这表明在非阻塞模式下调用了阻塞操作，该操作没有完成就返回这个错误
        // 此处保存这个错误 交给服务器处理
        if(len<=0){
            *saveerrno = errno;
            break;
        }
        if(_iov[0].iov_len+_iov[1].iov_len == 0){
            break;
        }

        // 此时说明iov[0]代表的写缓冲区已经写完了
        // iov[1]写入了一部分 截断一下防止重复
        if(_iov[0].iov_len < len){
            _iov[1].iov_base = _iov[1].iov_base +(len - _iov[0].iov_len);
            _iov[1].iov_len -= (len - _iov[0].iov_len);
            
            // 然后把写缓冲区重置一下 retriveve all
            // 顺便重置一下iov[0]
            if(_iov[0].iov_len){
                _writebuffer.retrieve_all();
                _iov[0].iov_len = 0;
            }
        }else{
            // 不然说明写缓冲区没写完，只写了一部分
            // 挪一下
            _iov[0].iov_base = _iov[0].iov_base+len;
            _iov[0].iov_len -= len;
            _writebuffer.retrieve(len);
        }
    // ET模式则一直循环，一次性把所有东西发完
    // 如果响应报文太大 也一次性发完
    // 水平模式就write一次就行
    }while(is_ET || to_write_bytes()>10240);
    return len;
}