#ifndef __EPOLLER__
#define __EPOLLER__

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <errno.h>
#include <stdio.h>  //perror

class Epoller{
public:
    explicit Epoller(int maxevent = 1024);
    ~Epoller();

    // add, mod, del
    bool add_fd(int fd,uint32_t events);
    bool mod_fd(int fd,uint32_t events);
    bool del_fd(int fd);

    int wait(int timeout_ms);

    // 从_events里获得 fd和event类型
    int get_eventfd(size_t i) const;
    uint32_t get_event(size_t i) const;

private:
    int _epollfd;
    std::vector<epoll_event> _events;
};

#endif // __EPOLLER__