#ifndef __MY_TASK__
#define __MY_TASK__

#include<functional>

class Task{
private:
    std::function<void()> task_func;
public:

    Task() = default;
    // 万能引用 左值复制 右值移动
    // template<class F>
    // Task(F&& func):task_func(std::forward<F>(func)){}

    template<class F>
    Task(F&& func){
        task_func = std::forward<F>(func);
    }

    Task(const Task& other){
        task_func = other.task_func;
    }

    /* 
    万能引用：即接受左值也接受右值。
    而该参数继而去调用Task的构造函数
    Task的构造函数也即支持左值也支持右值
    最终他会被转移到std::function身上。
    可以考虑一开始就转化为右值，让function去移动构造。
    但这样也会导致原先的左值失效。
    */

    template<class F>
    void set_task(F&& func){
        task_func = std::forward(func);
    }

    void process(){
        task_func();
    }

    // 用于判断threadPool是否关闭
    std::function<bool()> pool_is_closed = nullptr;
};

#endif // __MY_TASK__