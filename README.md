# MSGX - Cross-Platform Message Loop System & Thread Pool

# MSGX - 跨平台消息循环系统 & 线程池

## Overview / 概述

MSGX is a high-performance, cross-platform C++ library that provides a comprehensive message loop system and thread pool implementation. It supports asynchronous and synchronous task dispatching, timer management, and concurrent task processing with type-safe message passing.

MSGX 是一个高性能、跨平台的 C++ 库，提供完整的消息循环系统和线程池实现。它支持异步和同步任务分发、定时器管理，以及具有类型安全消息传递的并发任务处理。

## Key Features / 主要特性

### English
- **Generic Type Support**: Uses `std::any` or custom types for flexible parameter and return value handling
- **Asynchronous & Synchronous Operations**: Supports non-blocking `post` methods and blocking `send` methods
- **Timer Support**: Provides timer-based scheduling with one-time, fixed-rate, and fixed-delay modes
- **Thread Models**: Includes single-thread and thread pool implementations for concurrent task processing
- **Message Mapping**: Provides macro definitions for message and timer handlers, similar to MFC/Win32 message mapping
- **Thread Safety**: Ensures thread safety through `std::mutex` and `std::condition_variable`
- **Exception Handling**: Supports exception propagation in `send` methods and exception capture in `post_await`
- **Flexible Task Types**: Supports `std::function`, `std::packaged_task`, and custom messages

### 中文
- **泛型类型支持**：使用 `std::any` 或自定义类型实现灵活的参数和返回值处理
- **异步与同步操作**：支持非阻塞的 `post` 方法和阻塞的 `send` 方法
- **定时器支持**：提供基于定时器的调度，支持一次性、固定频率和固定延迟模式
- **线程模型**：包括单线程和线程池实现，支持并发任务处理
- **消息映射**：提供消息和定时器处理程序的宏定义，类似于 MFC/Win32 消息映射
- **线程安全**：通过 `std::mutex` 和 `std::condition_variable` 确保线程安全
- **异常处理**：支持 `send` 方法中的异常传播和 `post_await` 中的异常捕获
- **灵活的任务类型**：支持 `std::function`、`std::packaged_task` 和自定义消息

## Core Classes / 核心类

### 1. `_Message_loop` - Base Message Loop / 基础消息循环

#### Template Parameters / 模板参数
```cpp
template <
    typename _Arg_t = std::any,      // Parameter type / 参数类型
    typename _Result_t = std::any,   // Return type / 返回值类型
    typename _Key_t = uintptr_t      // Message key type / 消息键类型
>
```

#### Key Methods / 主要方法

##### Post Methods (Asynchronous) / Post 方法（异步）
```cpp
// Post task without return value / 投递无返回值任务
template <typename Function, typename... Args>
bool post(Function&& func, Args&&... args);

// Post task with return value / 投递有返回值任务
template <typename Function, typename... Args>
bool post(Function&& func, Args&&... args);

// Post message / 投递消息
bool post(_Key_t key, _Arg_t&& arg);

// Post with future return / 投递并返回 future
template <typename Function, typename... Args>
std::future<void> post_await(Function&& func, Args&&... args);

// Post with typed future return / 投递并返回类型化 future
template <typename Function, typename... Args>
std::future<_Result_t> post_await(Function&& func, Args&&... args);
```

##### Send Methods (Synchronous) / Send 方法（同步）
```cpp
// Send task without return value / 发送无返回值任务
template <typename Function, typename... Args>
void send(Function&& func, Args&&... args);

// Send task with return value / 发送有返回值任务
template <typename Function, typename... Args>
_Result_t send(Function&& func, Args&&... args);

// Send with timeout (relative time) / 发送带超时（相对时间）
template <typename Rep, typename Period, typename Function, typename... Args>
bool send(const std::chrono::duration<Rep, Period>& timeout, Function&& func, Args&&... args);

// Send with timeout (absolute time) / 发送带超时（绝对时间）
template <typename Clock, typename Duration, typename Function, typename... Args>
bool send(const std::chrono::time_point<Clock, Duration>& timeout, Function&& func, Args&&... args);

// Send message / 发送消息
_Result_t send(_Key_t key, _Arg_t&& arg);
```

##### Control Methods / 控制方法
```cpp
// Post quit message / 投递退出消息
void post_quit_message(bool cancel_pending = false);

// Check if disabled / 检查是否已禁用
bool disabled() const noexcept;

// Dispatch message loop / 运行消息循环
virtual void dispatch() noexcept;
```

### 2. `_Timer_message_loop` - Timer Support / 定时器支持

#### Template Parameters / 模板参数
```cpp
template <
    typename _Arg_t = std::any,
    typename _Result_t = std::any,
    typename _Key_t = uintptr_t,
    typename _Duration_t = std::chrono::nanoseconds,
    typename _Clock_t = std::chrono::steady_clock
>
```

#### Timer Modes / 定时器模式
```cpp
enum class schedule_mode {
    call_once,    // One-time execution / 一次性执行
    fixed_rate,   // Fixed rate scheduling / 固定频率调度
    fixed_delay   // Fixed delay scheduling / 固定延迟调度
};
```

#### Timer Methods / 定时器方法
```cpp
// Set timer with parameters / 设置带参数的定时器
template <typename Rep, typename Period>
bool set_timer(_Key_t key, const std::chrono::duration<Rep, Period>& elapse, 
               _Arg_t&& arg = {}, schedule_mode mode = schedule_mode::fixed_delay);

// Set one-time timer / 设置一次性定时器
template <typename Rep, typename Period>
bool set_timer_once(_Key_t key, const std::chrono::duration<Rep, Period>& elapse, 
                   _Arg_t&& arg = {});

// Set fixed delay timer / 设置固定延迟定时器
template <typename Rep, typename Period>
bool set_timer_fixed_delay(_Key_t key, const std::chrono::duration<Rep, Period>& elapse, 
                          _Arg_t&& arg = {});

// Set fixed rate timer / 设置固定频率定时器
template <typename Rep, typename Period>
bool set_timer_fixed_rate(_Key_t key, const std::chrono::duration<Rep, Period>& elapse, 
                         _Arg_t&& arg = {});

// Kill timer / 移除定时器
bool kill_timer(_Key_t key);
```

### 3. `_Thread_loop` - Single Thread Implementation / 单线程实现

#### Template Parameters / 模板参数
```cpp
template <typename _Base_t = _Message_loop<>>
```

#### Thread Management / 线程管理
```cpp
// Start thread / 启动线程
bool start();

// Stop thread / 停止线程
void stop(bool cancel_pending = false);

// Check if joinable / 检查是否可加入
bool joinable() const noexcept;

// Join thread / 等待线程结束
void join();
```

### 4. `_Thread_pool_loop` - Thread Pool Implementation / 线程池实现

#### Template Parameters / 模板参数
```cpp
template <typename _Base_t = _Message_loop<>>
```

#### Thread Pool Management / 线程池管理
```cpp
// Start thread pool / 启动线程池
bool start(std::size_t thread_count = std::thread::hardware_concurrency());

// Get thread count / 获取线程数
std::size_t get_thread_count() const;

// Stop thread pool / 停止线程池
void stop(bool cancel_pending = false);

// Check if joinable / 检查是否可加入
bool joinable() const noexcept;

// Join all threads / 等待所有线程结束
void join();
```

## Type Aliases / 类型别名

```cpp
namespace msgx {
    // Basic message loop / 基础消息循环
    template <typename Arg = std::any, typename Result = std::any, typename Key = uintptr_t>
    using message_loop = _Message_loop<Arg, Result, Key>;

    // Timer message loop / 定时器消息循环
    template <typename Arg = std::any, typename Result = std::any, typename Key = uintptr_t,
              typename Duration = std::chrono::nanoseconds, typename Clock = std::chrono::steady_clock>
    using timer_message_loop = _Timer_message_loop<Arg, Result, Key, Duration, Clock>;

    // Thread pool loop / 线程池循环
    template <typename Base = message_loop<>>
    using thread_pool_loop = _Thread_pool_loop<Base>;

    // Thread loop / 线程循环
    template <typename Base = message_loop<>>
    using thread_loop = _Thread_loop<Base>;
}
```

## Message Mapping Macros / 消息映射宏

### Message Mapping / 消息映射
```cpp
// Begin message map / 开始消息映射
XBEGIN_MSG_MAP()

// Message handler / 消息处理器
XMESSAGE_HANDLER(msgCode, func)

// Message range handler / 消息范围处理器
XMESSAGE_RANGE_HANDLER(msgFirst, msgLast, func)

// Chain to base class / 链接到基类
XCHAIN_MSG_MAP(_Base_class)

// Chain to member / 链接到成员
XCHAIN_MSG_MAP_MEMBER(_Member)

// End message map / 结束消息映射
XEND_MSG_MAP()
XEND_MSG_MAP_RETURN_BASE(_Base_class)
XEND_MSG_MAP_RETURN_VALUE(_Result)
```

### Timer Mapping / 定时器映射
```cpp
// Begin timer map / 开始定时器映射
XBEGIN_TIMER_MAP()

// Timer handler / 定时器处理器
XTIMER_HANDLER(timerId, func)

// Timer range handler / 定时器范围处理器
XTIMER_RANGE_HANDLER(timerBegin, timerEnd, func)

// Chain to base class / 链接到基类
XCHAIN_TIMER_MAP(_Base_class)

// Chain to member / 链接到成员
XCHAIN_TIMER_MAP_MEMBER(_Member)

// End timer map / 结束定时器映射
XEND_TIMER_MAP()
```

## Usage Examples / 使用示例

### Basic Message Loop / 基础消息循环
```cpp
#include "msgx.hpp"

// Create message loop / 创建消息循环
msgx::message_loop<> loop;

// Start in separate thread / 在单独线程中启动
std::thread t([&] { loop.dispatch(); });

// Post asynchronous task / 投递异步任务
loop.post([] { std::cout << "Hello from message loop!" << std::endl; });

// Send synchronous task / 发送同步任务
auto result = loop.send([] { return 42; });

// Post quit and join / 投递退出消息并等待
loop.post_quit_message();
t.join();
```

### Timer Message Loop / 定时器消息循环
```cpp
#include "msgx.hpp"

class MyTimerHandler : public msgx::timer_message_loop<>
{
protected:
    virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override {
        std::cout << "Timer " << key << " triggered!" << std::endl;
    }
};

MyTimerHandler handler;
std::thread t([&] { handler.dispatch(); });

// Set one-time timer / 设置一次性定时器
handler.set_timer_once(1, std::chrono::seconds(5));

// Set periodic timer / 设置周期性定时器
handler.set_timer_fixed_rate(2, std::chrono::seconds(1));

std::this_thread::sleep_for(std::chrono::seconds(10));
handler.post_quit_message();
t.join();
```

### Thread Pool / 线程池
```cpp
#include "msgx.hpp"

// Create thread pool / 创建线程池
msgx::thread_pool_loop<> pool;

// Start with 4 threads / 启动4个线程
pool.start(4);

// Post tasks / 投递任务
for (int i = 0; i < 10; ++i) {
    pool.post([i] { 
        std::cout << "Task " << i << " executed on thread " 
                  << std::this_thread::get_id() << std::endl; 
    });
}

// Stop and wait / 停止并等待
pool.stop();
pool.join();
```

### Message Mapping Example / 消息映射示例
```cpp
#include "msgx.hpp"

enum {
    MSG_HELLO = 1,
    MSG_WORLD = 2,
    MSG_QUIT = 3
};

class MyHandler : public msgx::thread_loop<msgx::timer_message_loop<>>
{
protected:
    XBEGIN_MSG_MAP()
        XMESSAGE_HANDLER(MSG_HELLO, OnHello)
        XMESSAGE_HANDLER(MSG_WORLD, OnWorld)
        XMESSAGE_HANDLER(MSG_QUIT, OnQuit)
        XEND_MSG_MAP()

    XBEGIN_TIMER_MAP()
        XTIMER_HANDLER(1, OnTimer)
        XEND_TIMER_MAP()

private:
    _TyResult OnHello(_TyKey key, _TyArg& arg) noexcept {
        std::cout << "Hello message received!" << std::endl;
        return {};
    }

    _TyResult OnWorld(_TyKey key, _TyArg& arg) noexcept {
        std::cout << "World message received!" << std::endl;
        return {};
    }

    _TyResult OnQuit(_TyKey key, _TyArg& arg) noexcept {
        post_quit_message();
        return {};
    }

    void OnTimer(_TyKey key, _TyArg& arg) noexcept {
        std::cout << "Timer triggered!" << std::endl;
    }
};

MyHandler handler;
handler.start();

// Send messages / 发送消息
handler.post(MSG_HELLO, std::string("Hello"));
handler.post(MSG_WORLD, std::string("World"));

// Set timer / 设置定时器
handler.set_timer_once(1, std::chrono::seconds(2));

// Send quit message / 发送退出消息
handler.post(MSG_QUIT, std::string("Quit"));
```

## Compilation Requirements / 编译要求

### English
- **C++17** or later
- **Standard Library**: Requires `std::any`, `std::optional`, `std::variant`, `std::future`, `std::thread`
- **Platform Libraries**: 
  - Linux: `pthread`, `atomic`
  - Windows: Standard Windows libraries
  - macOS: Standard macOS libraries

### 中文
- **C++17** 或更高版本
- **标准库**：需要 `std::any`、`std::optional`、`std::variant`、`std::future`、`std::thread`
- **平台库**：
  - Linux：`pthread`、`atomic`
  - Windows：标准 Windows 库
  - macOS：标准 macOS 库

## Performance Characteristics / 性能特征

### English
- **Zero-copy parameter passing** using `std::any` and move semantics
- **Efficient task queue management** with `std::deque`
- **Thread-safe operations** with minimal locking overhead
- **High-precision timer support** with nanosecond resolution
- **Scalable thread pool** with automatic hardware concurrency detection

### 中文
- **零拷贝参数传递**，使用 `std::any` 和移动语义
- **高效的任务队列管理**，使用 `std::deque`
- **线程安全操作**，锁开销最小
- **高精度定时器支持**，纳秒级分辨率
- **可扩展的线程池**，自动硬件并发检测

## Best Practices / 最佳实践

### English
1. **Use appropriate thread model**: Single thread for GUI applications, thread pool for server applications
2. **Handle exceptions properly**: Use `post_await` for exception capture, `send` for exception propagation
3. **Manage timer lifecycles**: Always call `kill_timer` for one-time timers that need early termination
4. **Use type-safe message keys**: Define enums for message identifiers
5. **Implement proper cleanup**: Always call `post_quit_message()` and `join()` before destruction

### 中文
1. **使用适当的线程模型**：GUI 应用使用单线程，服务器应用使用线程池
2. **正确处理异常**：使用 `post_await` 捕获异常，使用 `send` 传播异常
3. **管理定时器生命周期**：对于需要提前终止的一次性定时器，始终调用 `kill_timer`
4. **使用类型安全的消息键**：为消息标识符定义枚举
5. **实现适当的清理**：在销毁前始终调用 `post_quit_message()` 和 `join()`

## License / 许可证

This library is provided as-is for educational and development purposes. Please refer to the original source for licensing information.

本库按原样提供，用于教育和开发目的。请参考原始源代码了解许可信息。 