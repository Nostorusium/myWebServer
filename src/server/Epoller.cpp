#include "Epoller.hpp"
#include <iostream>

Epoller::Epoller(int maxevent):_events(maxevent){
    _epollfd = epoll_create(512);
    if(_epollfd<0 || _events.size()<=0){
        perror("epoller");
    }
}

Epoller::~Epoller(){
    close(_epollfd);
}

bool Epoller::add_fd(int fd,uint32_t events){
    if(fd<0) return false;
    epoll_event event = {0};
    event.data.fd = fd;
    event.events = events;

    int ret = epoll_ctl(_epollfd,EPOLL_CTL_ADD,fd,&event);
    if(ret==-1) return false;
    return true;
}

bool Epoller::mod_fd(int fd,uint32_t events){
    if(fd<0) return false;
    epoll_event event = {0};
    event.data.fd = fd;
    event.events = events;
    int ret = epoll_ctl(_epollfd,EPOLL_CTL_MOD,fd,&event);
    if(ret==-1) return false;
    return true;
}

bool Epoller::del_fd(int fd){
    if(fd<0) return false;
    epoll_event event = {0};
    int ret = epoll_ctl(_epollfd,EPOLL_CTL_DEL,fd,&event);
    if(ret==-1) return false;
    return true;
}

// return num of triggered events
int Epoller::wait(int timeout_ms){
    return epoll_wait(_epollfd,_events.data(),static_cast<int>(_events.size()),timeout_ms);
}

int Epoller::get_eventfd(size_t i) const{
    if(i>=_events.size() || i<0){
        return -1;
    }
    return _events[i].data.fd;
}

uint32_t Epoller::get_event(size_t i) const{
    if(i>=_events.size() || i<0){
        return -1;
    }
    return _events[i].events;
}