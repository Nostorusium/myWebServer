#ifndef __MY_THREADPOOL__
#define __MY_THREADPOOL__

#include<mutex>
#include<thread>
#include<vector>
#include<functional>
#include<atomic>
#include<future>
#include<condition_variable>
#include<list>
#include<iostream>

#include"Task.hpp"

class ThreadPool{
private:
    int _threadnum = 0;
    std::mutex _mutex;
    std::vector<std::shared_ptr<std::thread>> _threads;
    std::list<std::shared_ptr<Task>> _tasks;

    // 条件变量
    std::condition_variable cond;
    bool _isClosed = false;

    /*
    任务线程的要求：
    1. 阻塞式获得任务并执行任务体
    2. 能响应线程池的关闭结束执行
    */

    void threadFunc(){
        while(_isClosed == false){
            std::unique_lock<std::mutex> lock(_mutex);
            if(_tasks.empty()){
                cond.wait(lock);
            }
            // 条件变量的阻塞解除后，仍然可以被锁阻塞。
            // 当重新获得锁时，可能tasks又被其他线程取空了
            if(_tasks.empty()){
                continue;
            }
            // 不为空 取出task
            auto task = _tasks.front();
            _tasks.pop_front();
            task->process();
        }
    }
    
public:
    explicit ThreadPool() = default;

    explicit ThreadPool(int threadnum){
        _threadnum = threadnum;
        _threads.reserve(threadnum);
    }
    ~ThreadPool() = default;
    ThreadPool(const ThreadPool& other) = delete;
    ThreadPool &operator=(const ThreadPool &other) = delete;

    void start(){
        std::unique_lock<std::mutex> lock(_mutex);
        if(_threadnum<=0){
            perror("threadpool threadnum <= 0");
            return;
        }
        if(_threads.empty() == false){
            perror("threadpool is already running");
            return;
        }
        for(int i=0;i<_threadnum;i++){
            auto th = std::make_shared<std::thread>(&ThreadPool::threadFunc,this);
            _threads.push_back(std::move(th)); 
        }
    }
    void stop(){
        if(_isClosed){
            return;
        }
        _isClosed = true;
        cond.notify_all();
        for(auto& th:_threads){
            th->join();
        }

        // 象征性清空一下
        std::unique_lock<std::mutex> lock(_mutex);
        _threads.clear();
    }

    void add_task(const Task& task){
        std::unique_lock<std::mutex> lock(_mutex);

        auto new_task = std::make_shared<Task>(task);
        _tasks.push_back(new_task);
        lock.unlock();
        cond.notify_one();
    }

    bool is_closed(){
        return _isClosed;
    }

};  // threadpool

#endif //__MY_THREADPOOL__