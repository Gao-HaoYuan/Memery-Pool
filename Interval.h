#ifndef _INTERVAL_
#define _INTERVAL_

#include <thread>
#include <chrono>
#include <atomic>

//重命名 atomic_bool* 为 IntervalId
using IntervalId = std::atomic_bool*;

// 在指定间隔时间后执行特定操作函数
// 模板参数 F 为函数类型
// Args 为特定操作函数的形参，为可变参数
// 通过 bind 绑定操作函数和形参列表，并启动另一个线程，在指定的时间间隔后执行操作函数
// 创建的线程与主线程分离
template<class F, class ... Args>
void setTimeout(
    int delay,                              // I : 指定的时间间隔
    F &&function,                           // I : 传入的操作函数
    Args&& ... args                         // I ： 操作函数的参数列表
)
{
    // 通过 bind 绑定操作函数和形参
    auto cb = std::bind(std::forward<F>(function), std::forward<Args>(args)...);

    // 创建线程，并通过匿名函数执行指定的操作
    std::thread t([=]() {
        std::this_thread::sleep_for(std::chrono::seconds(delay));

        cb();
    });

    // 分离线程
    t.detach();
}

// 在指定间隔时间后执行特定操作函数
// 模板参数 F 为函数类型
// Args 为特定操作函数的形参，为可变参数
// 通过 bind 绑定操作函数和形参列表，并启动另一个线程，每次在指定的时间间隔后执行操作函数, 创建的线程与主线程分离
// Return : 操作函数执行停止的标记变量的，通过控制改量，可以使该进程停止运行
template<class F, class ... Args>
IntervalId setInterval(
    int interval,                               // I : 指定的时间间隔
    F &&function,                               // I : 传入的操作函数
    Args&& ... args                             // I ： 操作函数的参数列表
) 
{
    IntervalId active = new std::atomic_bool(true);
    // 通过 bind 绑定操作函数和形参
    auto cb = std::bind(std::forward<F>(function), std::forward<Args>(args)...);

    // 创建线程，并通过匿名函数执行指定的操作
    std::thread t([=]() {
        while(active->load()) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));

            if(active->load()){
                cb();
            }
        }
    });

    // 分离线程
    t.detach();

    return active;
}

// 将指定的 IntervalId 置为 false。终止定时器的运行
void clearInterval(
    IntervalId active               // I : 指定的定时器
)
{
    active->store(false);
}

#endif
