#ifndef __MY_BUFFER__
#define __MY_BUFFER__

#include<iostream>
#include<unistd.h>
#include<vector>
#include<atomic>
#include<sys/uio.h>

/*
该buffer主要用于处理fd的IO操作，提出几个指标
1. 从fd读入数据时，若不够放，可以动态调整大小
2. 良好的多线程安全性 atomic                
   如果Buffer被多线程调用，比如一个读一个写 对读写指针的修改应当是原子的                                      
3. 实现从fd读入buffer，与从buffer写入fd
*/

/*
    Memory be like:

    pending readable writable
       |    |         |
    -------------------------
buf |    |****|             |
    -------------------------
    ^    ^    ^             ^
    |    |    |             |
 begin read write        size:1024,1KB
*/
class Buffer{
private:
    std::vector<char> _buffer;
    std::atomic<std::size_t> _readpos;
    std::atomic<std::size_t> _writepos;

    char* _begin_ptr();
    const char* _begin_ptr() const;

    // 调整到开头位置
    void _adjust();

    // 调整空间大小
    void _expand_space(size_t size);

public:
    Buffer(int buffsize = 1024);

    ~Buffer() = default;

    size_t get_writable_bytes() const;
    size_t get_readable_bytes() const;
    size_t get_pending_bytes() const;

    // 添加数据
    void append(const char* data,size_t len);
    void append(const void* data,size_t len);
    void append(const std::string& str);

    // retrieve表示取走数据
    // 只是移动指针 并不会真的取走
    void retrieve(size_t len);
    void retrieve_until(const char* end);
    void retrieve_all();
    std::string retrieve_all_to_str();

    size_t read_from_fd(int fd,int* saveerrno);
    size_t write_to_fd(int fd,int* saveerrno);

    // 返回读指针，可以看一眼现在在读什么
    const char* peek() const;
};

#endif  // __MY_BUFFER__