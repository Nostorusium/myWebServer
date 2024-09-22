#include"Buffer.hpp"

/*
    Memory be like:

    pending readable writable
       |    |         |
    -------------------------
    |    |****|             |
    -------------------------
    ^    ^    ^             ^
    |    |    |             |
 begin read write        size:1024,1KB
*/

Buffer::Buffer(int buffsize):
    _buffer(buffsize,0),
    _readpos(0),
    _writepos(0){
}

size_t Buffer::get_writable_bytes() const{
    return _buffer.size() - _writepos.load();
}

size_t Buffer::get_readable_bytes() const{
    return _writepos.load()-_readpos.load();
}

size_t Buffer::get_pending_bytes() const{
    return _readpos.load();
}

char* Buffer::_begin_ptr(){
    return _buffer.data();
}

const char* Buffer::_begin_ptr() const{
    return _buffer.data();
}

const char* Buffer::peek() const{
    return _begin_ptr()+_readpos;
}

/*
    调整的时机：向后写入不够时，尝试一下调整
    如果还不够 需要申请额外空间
*/
void Buffer::_adjust(){
    size_t readable = get_readable_bytes();
    std::copy(_begin_ptr()+_readpos.load(),_begin_ptr()+_writepos.load(),_begin_ptr());
    _readpos.store(0);
    _writepos.store(_readpos.load() + readable);
}

// char* 字节流
void Buffer::append(const char* data,size_t len){
    // 向后写
    size_t rest = _buffer.size() - get_readable_bytes()-get_pending_bytes();

    // 不够写
    if(len>rest){
        // 所有可用
        size_t usable = get_writable_bytes()+get_pending_bytes();
        // 如果所有可用也不满足 那么申请额外空间
        if(usable < len){
            _expand_space(len);
        }
        _adjust();
    }

    // 够写
    std::copy(data,data+len,_begin_ptr()+_writepos);
    _writepos+=len;
}

// 任意类型
void Buffer::append(const void* data,size_t len){
    append((char*)data, len);
}

void Buffer::append(const std::string& str){
    append(str.data(),str.size());
}

void Buffer::_expand_space(size_t size){
    size_t curr_size = _buffer.size();
    // 给\n多保留一位
    _buffer.resize(curr_size+size+1);
}

// 读fd
size_t Buffer::write_to_fd(int fd,int* saveerrno){
    size_t readable_data = get_readable_bytes();
    size_t len = write(fd,peek(),readable_data);
    if(len<0){
        *saveerrno = errno;
        return len;
    }
    _readpos+= len;
    return len;
}

// 写入fd 每次写入的大小是有上限的 取决于缓冲区实际容量+temp大小
// 因为fd的写入需要直接写到某个缓冲区
// 在写好之前我们不能直接判断大小去扩容
size_t Buffer::read_from_fd(int fd,int* saveerrno){
    // 2^16B,64KB
    char temp[65535];
    // headfile: sys/uio.h
    iovec iovector[2];
    size_t writable = get_writable_bytes();

    // 往buffer里写的位置
    iovector[0].iov_base = _begin_ptr()+_writepos;
    iovector[0].iov_len = get_writable_bytes();
    // 第一个存不下就存到temp里 
    iovector[1].iov_base = temp;
    iovector[1].iov_len = sizeof(temp);

    // 开始从fd读，写入buffer
    ssize_t len = readv(fd,iovector,2);
    if(len<0){
        *saveerrno = errno;
        return len;
    }

    if(static_cast<size_t>(len) <= writable){
        _writepos += len;
    }else{
        // 说明第一个满了，已经存到temp里了
        _writepos = _buffer.size();
        append(temp,len-writable);
    }
    return len;
}

void Buffer::retrieve(size_t len){
    size_t readable = get_readable_bytes();
    if(len<readable){
        _readpos += len;
    }else{
        retrieve_all();
    }
}

void Buffer::retrieve_until(const char* end){
    if(peek()>end){
        perror("retrieve_until:peek>end");
        return;
    }
    retrieve(end-peek());
}

void Buffer::retrieve_all(){
    _readpos = 0;
    _writepos = 0;
}

std::string Buffer::retrieve_all_to_str(){
    std::string str(peek(),get_readable_bytes());
    retrieve_all();
    return str;
}