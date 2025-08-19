#pragma once

#include <cassert>

#include <any>
#include <future>
#include <functional>
#include <mutex>
#include <deque>
#include <variant>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <optional>
#include <chrono>

#define UNLOCK_AFTER_NOTIFY 1
#define COMPAT_CODE 1

/*
Linux 环境需要链接的库：
pthread
atomic
*/

/*
__________
\______   \ ____ _____    ____  ____   ____
 |    |  _// __ \\__  \ _/ ___\/  _ \ /    \
 |    |   \  ___/ / __ \\  \__(  <_> )   |  \
 |______  /\___  >____  /\___  >____/|___|  /
        \/     \/     \/     \/           \/
*/
/// <summary>
/// 跨平台消息循环系统 & 线程池
///     代码风格：C++STL
///     原类名：message_loop_core
///     -- Create.Beacon.20220906
///     -- Modify.Beacon.20220920
///         -- 1. 添加无消息ID的投递与发送函数
///         -- 2. 添加消息处理宏
///         -- 3. 增强类型
///         -- 4. 新增无Dispatch的处理与调用前处理
///         -- 5. 新增当前线程的Dispatch处理
///     -- Modify.Beacon.20230706
///         -- 1. 调整调度逻辑
///     -- Modify.Beacon.20230902
///         -- 1. 使用steady_clock避免因系统修改时间引发超时混乱
///     -- Modify.Beacon.20240514
///         -- 1. 添加调用基类的宏支持
///     -- Modify.Beacon.20240912
///         -- 1. 添加 post_callback_future：在队列尾部投递一个回调，返回Future，在任何位置等待其执行完毕
///         -- 2. 添加 post_callback：在队列尾部投递一个回调，不关心其什么时候执行完毕
///         -- 3. 添加 send_callback：在队列头插入一个回调，阻塞等待其执行完毕后返回其结果
///     -- Modify.Beacon.20241022
///         -- 1. 重义内部模板类型名称（应对某些残疾编译器编译问题）
///     -- Modify.Beacon.20241121
///         -- 1. 添加 _Key_t 模板参数，使消息ID和定时器ID的类型可以自定义
///         -- 2. 将 _Arg_t 参数定义为右值引用
///         -- 3. 定时器添加上下文参数
///     -- Modify.Beacon.20241122
///         -- 1. 将定时器结构使用 std::shared_ptr 维护，控制生命周期，使 kill_timer 和一次性定时器在使用lambda时不会崩溃
///     -- Modify.Beacon.20250328
///         -- 1. 添加多线程消费者支持 task_manager_core
///         -- 2. 去除 send_callback 超时函数中的一个没必要的模板参数
///     -- Modify.Beacon.20250421-20250427
///         -- 1. 重大重构
///     -- Modify.Beacon.20250509
///         -- 1. 修复BUG
///     -- Modify.Beacon.20250603
///         -- 1. 修复BUG：当某个ID值的Timer存在的时候，kill_timer后立马set_timer相同ID值。会导致前一个Timer继续存在
///     -- Modify.Beacon.20250616
///         -- 1. 对所有 send 函数添加超时能力
///         -- 2. 添加 disabled 函数，用于判断是否已经处在退出状态或已退出（禁用）
///     -- Modify.Beacon.20250811
///         -- 1. 对于一次性定时器，在触发完毕后，立即移除，而不是等到下次调度时才移除
///         -- 2. 对于多次设置相同ID的定时器，原逻辑会返回失败，现逻辑直接替换定时器配置信息（与 Win32API SetTimer 保持一致）

/*
主要特性
泛型支持：使用 std::any（或用户自定义类型）作为参数（_TyArg）、返回值（_TyResult）和键（_TyKey），提供灵活的消息传递机制。
异步与同步操作：支持异步的 post 方法（非阻塞）和同步的 send 方法（阻塞）执行任务。
定时器支持：提供基于定时器的调度，支持一次性（call_once）、固定频率（fixed_rate）和固定延迟（fixed_delay）模式。
线程模型：包括单线程（_Thread_loop）和线程池（_Thread_pool_loop）实现，支持并发任务处理。
消息映射：提供宏定义消息和定时器处理程序，类似于 MFC/Win32 消息映射，简化事件处理逻辑。
线程安全：通过 std::mutex 和 std::condition_variable 确保消息队列和定时器的线程安全。
异常处理：send 方法支持异常传播，post_await 和 post_await_cast 可捕获任务执行中的异常。
灵活的任务类型：支持 std::function、std::packaged_task 和自定义消息，允许多样化的任务处理。

_Message_loop 接口
post（无返回值任务）：异步投递无返回值任务，支持任意可调用对象和参数。
post（有返回值任务）：异步投递有返回值任务，返回值可转换为 _TyResult。
post（投递消息）：异步投递键值对消息，触发 _On_message 处理。
post_await（无返回值）：异步投递无返回值任务，返回 std::future<void> 以等待完成。
post_await（有返回值）：异步投递有返回值任务，返回 std::future<_TyResult> 获取结果。
post_await_cast：异步投递任务，返回精确类型 std::future（当 _TyResult 为 std::any）。
send（无返回值）：同步投递无返回值任务，阻塞直到任务完成。
send（有返回值，非 std::any）：同步投递有返回值任务，返回 _TyResult 类型结果。
send（有返回值，std::any）：同步投递有返回值任务，返回精确类型的 _TyResult。
send（发送消息）：同步发送键值对消息，返回 _On_message 的结果。
post_quit_message：投递退出消息，停止消息循环，支持可选取消未处理任务。
dispatch：运行消息循环，处理队列中的任务和消息，非阻塞实现。
_On_message：虚函数，处理消息回调，支持用户自定义消息处理逻辑。
Zero-copy 参数传递：利用 std::any 和移动语义减少参数拷贝开销。
异常传播：send 和 post_await 方法支持捕获和传播任务执行中的异常。
可重入任务投递：允许在消息处理中嵌套投递新任务或消息。

_Timer_message_loop 接口
set_timer（带参数）：设置定时器，指定触发时间、参数和调度模式（一次性、固定频率、固定延迟）。
set_timer（带回调）：设置定时器，指定回调函数和参数，支持复杂任务逻辑。
set_timer_once：设置一次性定时器，单次触发后自动移除。
set_timer_fixed_delay：设置固定延迟定时器，每次触发后重新计算间隔。
set_timer_fixed_rate：设置固定频率定时器，按固定时间间隔触发。
kill_timer：移除指定定时器，支持动态管理定时器生命周期。
dispatch：重写 _Message_loop 的 dispatch，集成定时器调度和消息处理。
_On_timer：虚函数，处理定时器事件回调，支持用户自定义定时器逻辑。
定时器优先级管理：内部使用优先级队列确保定时器按时间顺序触发。
非阻塞定时器调度：定时器触发不阻塞主消息循环，保证实时性。
自定义时钟支持：通过 _Clock_t 模板参数支持用户指定时钟类型。
可配置定时器精度：通过 _Duration_t 模板参数支持不同时间精度。

_Thread_loop 接口
start：启动单线程运行消息循环，隔离主线程操作。
stop：停止消息循环并等待线程结束，清理资源。
joinable：检查线程是否可加入，判断运行状态。
join：等待线程结束，确保线程安全退出。
线程安全任务分发：单线程模型确保任务顺序执行，无并发竞争。

_Thread_pool_loop 接口
start：启动线程池，指定线程数量，自动适配硬件并发性。
stop：停止线程池并等待所有线程结束，清理资源。
get_thread_count：获取当前线程池的线程数，支持运行时查询。
joinable：检查是否存在可加入的线程，判断运行状态。
join：等待所有线程结束，确保线程池安全退出。
线程池动态调整：根据硬件并发性优化线程数量，提升资源利用率。
并发任务处理：支持多线程并发执行任务，优化高负载场景。

消息映射宏
XBEGIN_MSG_MAP：开始消息映射定义，初始化消息处理逻辑。
XMESSAGE_HANDLER：为特定消息键绑定处理函数，支持单一消息处理。
XMESSAGE_RANGE_HANDLER：为消息键范围绑定处理函数，支持批量消息处理。
XCHAIN_MSG_MAP：调用基类的消息处理，支持继承链处理。
XCHAIN_MSG_MAP_MEMBER：调用成员对象的消息处理，支持组合模式。
XEND_MSG_MAP：结束消息映射，返回默认值，完成处理逻辑。
XEND_MSG_MAP_RETURN_BASE：结束消息映射，返回基类处理结果。
XEND_MSG_MAP_RETURN_VALUE：结束消息映射，返回用户指定值。

定时器映射宏
XBEGIN_TIMER_MAP：开始定时器映射定义，初始化定时器处理逻辑。
XTIMER_HANDLER：为特定定时器键绑定处理函数，支持单一定时器处理。
XTIMER_RANGE_HANDLER：为定时器键范围绑定处理函数，支持批量定时器处理。
XCHAIN_TIMER_MAP：调用基类的定时器处理，支持继承链处理。
XCHAIN_TIMER_MAP_MEMBER：调用成员对象的定时器处理，支持组合模式。
XEND_TIMER_MAP：结束定时器映射，完成定时器处理逻辑。

*/

namespace msgx {
    namespace detail {

        template<typename... Args>
        inline auto _Tie_tuple(std::tuple<Args...>& t) {
            return std::apply([](Args&... args) {
                return std::tie(args...);
                }, t);
        }

        template<typename... Args>
        inline auto _Tie_const_tuple(const std::tuple<Args...>& t) {
            return std::apply([](const Args&... args) {
                return std::tie(args...);
                }, t);
        }

        template <std::size_t N, typename Tuple, std::size_t... Is>
        inline auto _Tuple_head_impl(Tuple&& t, std::index_sequence<Is...>) {
            return std::make_tuple(std::get<Is>(std::forward<Tuple>(t))...);
        }

        template <std::size_t N, typename Tuple>
        inline auto _Tuple_head(Tuple&& t) {
            return _Tuple_head_impl<N>(
                std::forward<Tuple>(t),
                std::make_index_sequence<N>{}
            );
        }

        template <std::size_t N, typename Tuple, std::size_t... Is>
        inline auto _Tuple_head_refs_impl(Tuple& t, std::index_sequence<Is...>) {
            return std::tie(std::get<Is>(t)...);
        }

        template <std::size_t N, typename Tuple>
        inline auto _Tuple_head_refs(Tuple& t) {
            return _Tuple_head_refs_impl<N>(t, std::make_index_sequence<N>{});
        }
    }
    /*
    ## 1. _Message_loop

    **简介**：
    `_Message_loop` 是 `msgx` 库的核心类，提供基础的消息循环机制，用于异步和同步任务分发。它支持通过键值对消息或可调用对象投递任务，适用于事件驱动编程。类通过泛型模板（`_Arg_t`、`_Result_t`、`_Key_t`）实现灵活的参数和返回值处理，内置线程安全队列，适合构建轻量级事件处理框架。

    **主要特性**：
    - 异步投递（`post`）和同步执行（`send`）任务或消息。
    - 支持 `std::any` 或自定义类型的参数和返回值。
    - 提供虚函数 `_On_message` 用于自定义消息处理。
    - 高效的任务队列管理和线程安全操作。

    - **_Arg_t**
    - **用途**：指定消息或任务的参数类型，传递给任务回调或 `_On_message` 方法。
    - **默认值**：`std::any`，允许存储任意类型参数，支持动态类型处理。
    - **使用场景**：
        - 使用 `std::any` 适用于需要灵活传递不同类型参数的场景（如字符串、整数、自定义结构体）。
        - 可指定具体类型（如 `std::string`）以限制参数类型并提高类型安全性。
        - 影响 `post`、`send` 和 `_On_message` 的参数传递方式。

    - **_Result_t**
    - **用途**：指定任务或消息处理的返回值类型，返回给调用者或 `_On_message` 方法。
    - **默认值**：`std::any`，允许返回任意类型结果，支持动态类型处理。
    - **使用场景**：
        - 使用 `std::any` 适用于需要返回不同类型结果的场景（如整数、字符串、向量）。
        - 可指定具体类型（如 `int`）以限制返回值类型并提高类型安全性。
        - 影响 `send`、`post_await` 和 `_On_message` 的返回值处理。

    - **_Key_t**
    - **用途**：指定消息的键类型，用于标识消息或任务，传递给 `_On_message` 方法。
    - **默认值**：`uintptr_t`，提供整数类型的消息标识，适合枚举值或常量。
    - **使用场景**：
        - 使用 `uintptr_t` 适用于简单消息标识（如枚举值 `CM_MESSAGE0`）。
        - 可指定其他类型（如 `std::string`）以支持更复杂的消息键（如字符串标识）。
        - 影响 `post`、`send` 和 `_On_message` 的消息键处理。
    */
    template <
        typename _Arg_t = std::any,
        typename _Result_t = std::any,
        typename _Key_t = uintptr_t
    >
    class _Message_loop
    {
    public:
        using _TyKey = _Key_t;
        using _TyArg = _Arg_t;
        using _TyResult = _Result_t;

        enum {
            IDX_PROMISE_RESULT = 2,
        };

        using _TyFunctionWithResult = std::function<_TyResult()>;
        using _TyFunctionWithoutResult = std::function<void()>;
        using _TyPackagedTaskWithResult = std::packaged_task<_TyResult()>;
        using _TyPackagedTaskWithoutResult = std::packaged_task<void()>;
        using _TyMessageWithResult = std::tuple<_TyKey, _TyArg, std::promise<_TyResult>>;
        using _TyMessageWithoutResult = std::tuple<_TyKey, _TyArg>;

        using _TyItem = std::variant<
            std::monostate,
            _TyMessageWithResult,
            _TyMessageWithoutResult,
            _TyFunctionWithResult,
            _TyFunctionWithoutResult,
            _TyPackagedTaskWithResult,
            _TyPackagedTaskWithoutResult
        >;

        using _TyMutex = std::mutex;
    protected:

        /*
        * ### _On_message
        * - **功能**：虚函数，处理投递的消息，供用户重写以实现自定义消息逻辑。
        * - **参数**：
        *   - `key`: 消息标识（类型为 _Key_t），区分消息类型。
        *   - `arg`: 消息参数（类型为 _TyArg），传递的消息内容。
        * - **返回值**：`_TyResult`，消息处理的结果。
        */
        virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept { return {}; }
    public:
        virtual ~_Message_loop() = default;
    public:
        /*
        * ### post（无返回值任务）
        * - **功能**：异步投递一个无返回值的任务到消息队列，任务在 dispatch 调用时执行。
        * - **参数**：
        *   - `func`: 可调用对象（如函数、lambda），定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`bool`，表示任务是否成功投递到队列。
        */
        template <typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        bool post(__Function_t&& func, __Args_t&&... args) {
            return _Push_back(
                _TyFunctionWithoutResult(
                    std::bind(
                        std::forward<__Function_t>(func),
                        std::forward<__Args_t>(args)...
                    )
                )
            );
        }

        /*
        * ### post（有返回值任务）
        * - **功能**：异步投递一个有返回值的任务到消息队列，任务在 dispatch 调用时执行。
        * - **参数**：
        *   - `func`: 可调用对象，定义任务逻辑，返回值需兼容 _TyResult。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`bool`，表示任务是否成功投递到队列。
        */
        template <typename __Function_t, typename... __Args_t,
            std::enable_if_t<!std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        bool post(__Function_t&& func, __Args_t&&... args) {
            return _Push_back(
                _TyFunctionWithResult(
                    std::bind(
                        std::forward<__Function_t>(func),
                        std::forward<__Args_t>(args)...
                    )
                )
            );
        }

        /*
        * ### post_await（无返回值）
        * - **功能**：异步投递一个无返回值的任务，并返回 std::future 以等待任务完成。
        * - **参数**：
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::future<void>`，用于等待任务完成或捕获异常。
        */
        template <typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        std::future<void> post_await(__Function_t&& func, __Args_t&&... args) {
            auto _Task = _TyPackagedTaskWithoutResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_back(std::move(_Task))) {
                return {};
            }

            return _Future;
        }

        /*
        * ### post_await（有返回值）
        * - **功能**：异步投递一个有返回值的任务，并返回 std::future 以获取结果。
        * - **参数**：
        *   - `func`: 可调用对象，定义任务逻辑，返回值需兼容 _TyResult。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::future<_TyResult>`，用于获取任务结果或捕获异常。
        */
        template <typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_convertible_v<std::invoke_result_t<__Function_t, __Args_t...>, _TyResult>, int> = 0>
        std::future<_TyResult> post_await(__Function_t&& func, __Args_t&&... args) {
            auto _Task = _TyPackagedTaskWithResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_back(std::move(_Task))) {
                return {};
            }

            return _Future;
        }

        /*
        * ### post_await_cast
        * - **功能**：异步投递任务并返回精确类型的 std::future（当 _TyResult 为 std::any）。
        * - **参数**：
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::future<std::invoke_result_t<__Function_t, __Args_t...>>`，提供精确的任务结果类型。
        */
        template<
            typename __Function_t,
            typename... __Args_t,
            std::enable_if_t<
            std::is_convertible_v<std::invoke_result_t<__Function_t, __Args_t...>, _TyResult>&&
            std::is_same_v<std::decay_t<_TyResult>, std::any>,
            int
            > = 0
        >
        std::future<std::invoke_result_t<__Function_t, __Args_t...>> post_await_cast(__Function_t&& func, __Args_t&&... args) {
            using _RealResult_t = std::invoke_result_t<__Function_t, __Args_t...>;

            auto _Promise_result_ptr = std::make_shared<std::promise<_RealResult_t>>();
            auto _Future_result = _Promise_result_ptr->get_future();

            auto _Future_any = post_await(
                std::forward<__Function_t>(func),
                std::forward<__Args_t>(args)...
            );
            auto _Future_any_ptr = std::make_shared<std::decay_t<decltype(_Future_any)>>(std::move(_Future_any));

            // std::move_only_function in C++23
            auto _Ok = post(
                [](std::decay_t<decltype(_Future_any_ptr)> _Future_any_ptr, std::decay_t<decltype(_Promise_result_ptr)> _Promise_result_ptr)
                {
                    try {
                        std::any _Res_any = _Future_any_ptr->get();
                        _Promise_result_ptr->set_value(std::any_cast<_RealResult_t>(std::move(_Res_any)));
                    }
                    catch (...) {
                        _Promise_result_ptr->set_exception(std::current_exception());
                    }
                }, std::move(_Future_any_ptr), std::move(_Promise_result_ptr)
                    );

            if (!_Ok) {
                std::promise<_RealResult_t> _Promise_error;
                _Promise_error.set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("Failed to push task to queue")
                    )
                );
                return _Promise_error.get_future();
            }

            return _Future_result;
        }

        /*
        * ### post（投递消息）
        * - **功能**：异步投递一个键值对消息到消息队列，触发 _On_message 处理。
        * - **参数**：
        *   - `_Key`: 消息标识（类型为 _Key_t），用于区分消息类型。
        *   - `_Arg`: 消息参数（类型为 _TyArg），传递给 _On_message。
        * - **返回值**：`bool`，表示消息是否成功投递到队列。
        */
        bool post(_Key_t _Key, _Arg_t&& _Arg) {
            return _Push_back(_TyMessageWithoutResult{ _Key, _Arg });
        }
    public:

        /*
        * ### send（无返回值）
        * - **功能**：同步执行一个无返回值的任务，阻塞直到任务完成。
        * - **参数**：
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        */
        template <typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        void send(__Function_t&& func, __Args_t&&... args) {
            auto _Task = _TyPackagedTaskWithoutResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            _Future.get();
        }

        /*
        * ### send（无返回值，相对时间超时）
        * - **功能**：同步执行一个无返回值的任务，带相对时间超时，阻塞直到任务完成或超时。
        * - **参数**：
        *   - `_Rel_time`: 相对时间超时（类型为 std::chrono::duration），指定等待时间。
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`bool`，若任务在超时时间内完成返回 true，否则返回 false。
        */
        template <
            class __Rep, class __Per,
            typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        bool send(
            const std::chrono::duration<__Rep, __Per>& _Rel_time,
            __Function_t&& func, __Args_t&&... args
        ) {
            auto _Task = _TyPackagedTaskWithoutResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            return _Future.wait_for(_Rel_time) == std::future_status::ready;
        }

        /*
        * ### send（无返回值，绝对时间超时）
        * - **功能**：同步执行一个无返回值的任务，带绝对时间超时，阻塞直到任务完成或超时。
        * - **参数**：
        *   - `_Abs_time`: 绝对时间超时（类型为 std::chrono::time_point），指定截止时间。
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`bool`，若任务在截止时间前完成返回 true，否则返回 false。
        */
        template <
            class __Clock, class __Dur,
            typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        bool send(
            const std::chrono::time_point<__Clock, __Dur>& _Abs_time,
            __Function_t&& func, __Args_t&&... args
        ) {
            auto _Task = _TyPackagedTaskWithoutResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            return _Future.wait_until(_Abs_time) == std::future_status::ready;
        }

        /*
        * ### send（有返回值，非 std::any）
        * - **功能**：同步执行一个有返回值的任务，阻塞直到任务完成。
        * - **参数**：
        *   - `func`: 可调用对象，定义任务逻辑，返回值需兼容 _TyResult。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`__Result_t`，任务的返回值。
        */
        template <typename __Function_t, typename... __Args_t, typename __Result_t = _TyResult,
            std::enable_if_t<std::is_convertible_v<std::invoke_result_t<__Function_t, __Args_t...>, __Result_t> && !std::is_same_v<__Result_t, std::any>, int> = 0>
        __Result_t send(__Function_t&& func, __Args_t&&... args) noexcept(false) {
            auto _Task = _TyPackagedTaskWithResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            return _Future.get();
        }

        /*
        * ### send（有返回值，非 std::any，相对时间超时）
        * - **功能**：同步执行一个有返回值的任务，带相对时间超时，阻塞直到任务完成或超时。
        * - **参数**：
        *   - `_Rel_time`: 相对时间超时（类型为 std::chrono::duration），指定等待时间。
        *   - `func`: 可调用对象，定义任务逻辑，返回值需兼容 _TyResult。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::optional<__Result_t>`，若任务完成返回结果，否则返回 std::nullopt。
        */
        template <
            class __Rep, class __Per,
            typename __Function_t, typename... __Args_t, typename __Result_t = _TyResult,
            std::enable_if_t<std::is_convertible_v<std::invoke_result_t<__Function_t, __Args_t...>, __Result_t> && !std::is_same_v<__Result_t, std::any>, int> = 0>
        std::optional<__Result_t> send(
            const std::chrono::duration<__Rep, __Per>& _Rel_time,
            __Function_t&& func, __Args_t&&... args) noexcept(false) {
            auto _Task = _TyPackagedTaskWithResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            if (_Future.wait_for(_Rel_time) == std::future_status::ready) {
                return _Future.get();
            }

            return std::nullopt;
        }

        /*
        * ### send（有返回值，非 std::any，绝对时间超时）
        * - **功能**：同步执行一个有返回值的任务，带绝对时间超时，阻塞直到任务完成或超时。
        * - **参数**：
        *   - `_Abs_time`: 绝对时间超时（类型为 std::chrono::time_point），指定截止时间。
        *   - `func`: 可调用对象，定义任务逻辑，返回值需兼容 _TyResult。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::optional<__Result_t>`，若任务完成返回结果，否则返回 std::nullopt。
        */
        template <
            class __Clock, class __Dur,
            typename __Function_t, typename... __Args_t, typename __Result_t = _TyResult,
            std::enable_if_t<std::is_convertible_v<std::invoke_result_t<__Function_t, __Args_t...>, __Result_t> && !std::is_same_v<__Result_t, std::any>, int> = 0>
        std::optional<__Result_t> send(
            const std::chrono::time_point<__Clock, __Dur>& _Abs_time,
            __Function_t&& func, __Args_t&&... args) noexcept(false) {
            auto _Task = _TyPackagedTaskWithResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            if (_Future.wait_until(_Abs_time) == std::future_status::ready) {
                return _Future.get();
            }

            return std::nullopt;
        }

        /*
        * ### send（有返回值，std::any）
        * - **功能**：同步执行一个有返回值的任务（当 _TyResult 为 std::any），阻塞直到完成。
        * - **参数**：
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::invoke_result_t<__Function_t, __Args_t...>`，任务的精确返回值类型。
        */
        template <typename __Function_t, typename... __Args_t, typename __Result_t = _TyResult,
            std::enable_if_t<std::is_same_v<__Result_t, std::any>, int> = 0>
        std::invoke_result_t<__Function_t, __Args_t...> send(__Function_t&& func, __Args_t&&... args) noexcept(false) {
            auto _Task = _TyPackagedTaskWithResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            std::invoke_result_t<__Function_t, __Args_t...> _Result = {};
            try {
                _Result = std::any_cast<decltype(_Result)>(_Future.get());
            }
            catch (const std::bad_any_cast& e) {
                throw std::runtime_error(
                    std::string("Failed to cast result to specified ReturnType: ") + e.what()
                );
            }
            catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string("Task execution failed: ") + e.what()
                );
            }

            return _Result;
        }

        /*
        * ### send（有返回值，std::any，相对时间超时）
        * - **功能**：同步执行一个有返回值的任务（当 _TyResult 为 std::any），带相对时间超时，阻塞直到完成或超时。
        * - **参数**：
        *   - `_Rel_time`: 相对时间超时（类型为 std::chrono::duration），指定等待时间。
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::optional<std::invoke_result_t<__Function_t, __Args_t...>>`，若任务完成返回结果，否则返回 std::nullopt。
        */
        template <
            class __Rep, class __Per,
            typename __Function_t, typename... __Args_t, typename __Result_t = _TyResult,
            std::enable_if_t<std::is_same_v<__Result_t, std::any>, int> = 0>
        std::optional<std::invoke_result_t<__Function_t, __Args_t...>> send(
            const std::chrono::duration<__Rep, __Per>& _Rel_time,
            __Function_t&& func, __Args_t&&... args
        ) noexcept(false) {
            auto _Task = _TyPackagedTaskWithResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            if (_Future.wait_for(_Rel_time) != std::future_status::ready) {
                return std::nullopt;
            }

            std::invoke_result_t<__Function_t, __Args_t...> _Result = {};
            try {
                _Result = std::any_cast<decltype(_Result)>(_Future.get());
            }
            catch (const std::bad_any_cast& e) {
                throw std::runtime_error(
                    std::string("Failed to cast result to specified ReturnType: ") + e.what()
                );
            }
            catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string("Task execution failed: ") + e.what()
                );
            }

            return _Result;
        }

        /*
        * ### send（有返回值，std::any，绝对时间超时）
        * - **功能**：同步执行一个有返回值的任务（当 _TyResult 为 std::any），带绝对时间超时，阻塞直到完成或超时。
        * - **参数**：
        *   - `_Abs_time`: 绝对时间超时（类型为 std::chrono::time_point），指定截止时间。
        *   - `func`: 可调用对象，定义任务逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`std::optional<std::invoke_result_t<__Function_t, __Args_t...>>`，若任务完成返回结果，否则返回 std::nullopt。
        */
        template <
            class __Clock, class __Dur,
            typename __Function_t, typename... __Args_t, typename __Result_t = _TyResult,
            std::enable_if_t<std::is_same_v<__Result_t, std::any>, int> = 0>
        std::optional<std::invoke_result_t<__Function_t, __Args_t...>> send(
            const std::chrono::time_point<__Clock, __Dur>& _Abs_time,
            __Function_t&& func, __Args_t&&... args
        ) noexcept(false) {
            auto _Task = _TyPackagedTaskWithResult(
                std::bind(
                    std::forward<__Function_t>(func),
                    std::forward<__Args_t>(args)...
                )
            );
            auto _Future = _Task.get_future();
            if (!_Push_front(std::move(_Task))) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            if (_Future.wait_until(_Abs_time) != std::future_status::ready) {
                return std::nullopt;
            }

            std::invoke_result_t<__Function_t, __Args_t...> _Result = {};
            try {
                _Result = std::any_cast<decltype(_Result)>(_Future.get());
            }
            catch (const std::bad_any_cast& e) {
                throw std::runtime_error(
                    std::string("Failed to cast result to specified ReturnType: ") + e.what()
                );
            }
            catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string("Task execution failed: ") + e.what()
                );
            }

            return _Result;
        }
        /*
        * ### send（发送消息）
        * - **功能**：同步发送一个键值对消息，阻塞直到 _On_message 处理完成。
        * - **参数**：
        *   - `_Key`: 消息标识（类型为 _Key_t），用于区分消息类型。
        *   - `_Arg`: 消息参数（类型为 _TyArg），传递给 _On_message。
        * - **返回值**：`_TyResult`，_On_message 处理的结果。
        */
        _TyResult send(_TyKey _Key, _TyArg&& _Arg) noexcept(false) {
            std::promise<_TyResult> _Promise_result;
            auto _Future_result = _Promise_result.get_future();
            if (!_Push_front(_TyMessageWithResult{ _Key, _Arg, std::move(_Promise_result) })) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            return _Future_result.get();
        }

        /*
        * ### send（发送消息，相对时间超时）
        * - **功能**：同步发送一个键值对消息，带相对时间超时，阻塞直到 _On_message 处理完成或超时。
        * - **参数**：
        *   - `_Rel_time`: 相对时间超时（类型为 std::chrono::duration），指定等待时间。
        *   - `_Key`: 消息标识（类型为 _Key_t），用于区分消息类型。
        *   - `_Arg`: 消息参数（类型为 _TyArg），传递给 _On_message。
        * - **返回值**：`std::optional<_TyResult>`，若处理完成返回 _On_message 的结果，否则返回 std::nullopt。
        */
        template <class __Rep, class __Per>
        std::optional<_TyResult> send(
            const std::chrono::duration<__Rep, __Per>& _Rel_time,
            _TyKey _Key, _TyArg&& _Arg
        ) noexcept(false) {
            std::promise<_TyResult> _Promise_result;
            auto _Future_result = _Promise_result.get_future();
            if (!_Push_front(_TyMessageWithResult{ _Key, _Arg, std::move(_Promise_result) })) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            if (_Future_result.wait_for(_Rel_time) == std::future_status::ready) {
                return _Future_result.get();
            }

            return std::nullopt;
        }

        /*
        * ### send（发送消息，绝对时间超时）
        * - **功能**：同步发送一个键值对消息，带绝对时间超时，阻塞直到 _On_message 处理完成或超时。
        * - **参数**：
        *   - `_Abs_time`: 绝对时间超时（类型为 std::chrono::time_point），指定截止时间。
        *   - `_Key`: 消息标识（类型为 _Key_t），用于区分消息类型。
        *   - `_Arg`: 消息参数（类型为 _TyArg），传递给 _On_message。
        * - **返回值**：`std::optional<_TyResult>`，若处理完成返回 _On_message 的结果，否则返回 std::nullopt。
        */
        template <class __Clock, class __Dur>
        std::optional<_TyResult> send(
            const std::chrono::time_point<__Clock, __Dur>& _Abs_time,
            _TyKey _Key, _TyArg&& _Arg
        ) noexcept(false) {
            std::promise<_TyResult> _Promise_result;
            auto _Future_result = _Promise_result.get_future();
            if (!_Push_front(_TyMessageWithResult{ _Key, _Arg, std::move(_Promise_result) })) {
                throw std::runtime_error("Failed to push task to queue: queue might be in shutdown state");
            }

            if (_Future_result.wait_until(_Abs_time) == std::future_status::ready) {
                return _Future_result.get();
            }

            return std::nullopt;
        }
    public:

        /*
        * ### post_quit_message
        * - **功能**：投递退出消息，停止消息循环，可选择取消未处理任务。
        * - **参数**：
        *   - `_Cancel_pending`: 布尔值，若为 true，清空队列中未处理的任务，默认为 false。
        */
        void post_quit_message(bool _Cancel_pending = false)
        {
            std::scoped_lock<_TyMutex> locker(_My_mtx);
            _My_quit_sign = true;
            if (_Cancel_pending) {
                _My_items.clear();
            }

            _My_cv.notify_all();
        }

        /*
        * ### disabled
        * - **功能**：检查消息循环是否处于禁用状态（已退出或正在退出）。
        * - **参数**：无。
        * - **返回值**：`bool`，若消息循环已禁用（退出标志已设置）则返回 true。
        */
        bool disabled() const noexcept {
            std::scoped_lock<_TyMutex> locker(_My_mtx);
            return _My_quit_sign;
        }
    protected:

        /*
        * ### _Modify
        * - **功能**：线程安全地修改消息队列，执行指定的推送操作。
        * - **参数**：
        *   - `_Modify_function`: 要执行的修改操作（lambda 或函数对象）。
        * - **返回值**：`bool`，若操作成功（未处于退出状态）返回 true，否则返回 false。
        */
        bool _Modify(std::function<void()> _Modify_function)
        {
            std::unique_lock<_TyMutex> locker(_My_mtx, std::defer_lock);
            locker.lock();
            if (_My_quit_sign) {
                return false;
            }

            _Modify_function();

#if UNLOCK_AFTER_NOTIFY
            locker.unlock();
#endif
            _My_cv.notify_one();
            return true;
        }

        /*
        * ### _Push_back
        * - **功能**：将任务或消息添加到队列尾部。
        * - **参数**：
        *   - `task`: 要添加的任务或消息（类型为 _TyItem）。
        * - **返回值**：`bool`，若成功添加到队列返回 true，否则返回 false。
        */
        bool _Push_back(_TyItem&& task) {
            return _Modify([&] { _My_items.push_back(std::move(task)); });
        }

        /*
        * ### _Push_front
        * - **功能**：将任务或消息添加到队列头部，优先执行。
        * - **参数**：
        *   - `task`: 要添加的任务或消息（类型为 _TyItem）。
        * - **返回值**：`bool`，若成功添加到队列返回 true，否则返回 false。
        */
        bool _Push_front(_TyItem&& task) {
            return _Modify([&] { _My_items.push_front(std::move(task)); });
        }

        /*
        * ### _Do
        * - **功能**：执行单个任务或消息，处理不同的任务类型。
        * - **参数**：
        *   - `_Task_rref`: 要执行的任务或消息（类型为 _TyItem）。
        */
        inline void _Do(_TyItem&& _Task_rref) {
            if (auto _Callback_ptr = std::get_if<_TyPackagedTaskWithResult>(&_Task_rref)) {
                (*_Callback_ptr)();
            }
            else if (auto _Callback_ptr = std::get_if<_TyPackagedTaskWithoutResult>(&_Task_rref)) {
                (*_Callback_ptr)();
            }
            else if (auto _Callback_ptr = std::get_if<_TyFunctionWithResult>(&_Task_rref)) {
                (*_Callback_ptr)();
            }
            else if (auto _Callback_ptr = std::get_if<_TyFunctionWithoutResult>(&_Task_rref)) {
                (*_Callback_ptr)();
            }
            else if (auto _Message_ptr = std::get_if<_TyMessageWithResult>(&_Task_rref)) {
                auto _Args = std::tuple_cat(std::make_tuple(this), detail::_Tuple_head_refs<2>(*_Message_ptr));
                auto _Result = std::apply(std::mem_fn(&_Message_loop::_On_message), _Args);
                std::get<IDX_PROMISE_RESULT>(*_Message_ptr).set_value(_Result);
            }
            else if (auto _Message_ptr = std::get_if<_TyMessageWithoutResult>(&_Task_rref)) {
                auto _Args = std::tuple_cat(std::make_tuple(this), detail::_Tie_tuple(*_Message_ptr));
                std::apply(std::mem_fn(&_Message_loop::_On_message), _Args);
            }
        }
    public:

        /*
        * ### dispatch
        * - **功能**：运行消息循环，处理队列中的任务和消息，直到接收到退出消息。
        * - **参数**：无。
        */
        virtual void dispatch() noexcept
        {
            for (;;)
            {
                std::unique_lock<_TyMutex> locker(_My_mtx);
                // Wait for the event

                _My_cv.wait(locker,
                    [&] {
                        return
                            !_My_items.empty() ||
                            _My_quit_sign;
                    }
                );

                // ============================================================
                // Task Start
                if (_My_quit_sign && _My_items.empty()) {
                    // 如果设置了退出标志，并且任务队列为空则退出调度
                    break;
                }

                // 处理一个消息
                auto _Task = std::move(_My_items.front());
                _My_items.pop_front();
                locker.unlock();
                _Do(std::move(_Task));
                // Task End
                // ============================================================
            }
        }
    protected:
        // Tasks & Messages
        std::deque<_TyItem> _My_items;

        mutable _TyMutex _My_mtx;
        std::condition_variable _My_cv;
        bool _My_quit_sign{ false };
    };
    /*
    ## 2. _Timer_message_loop

    **简介**：
    `_Timer_message_loop` 继承自 `_Message_loop`，扩展了定时器调度功能，支持一次性、固定频率和固定延迟的定时任务。它允许用户设置定时器以触发回调或消息，适用于周期性任务或延迟执行场景。类通过 `_Duration_t` 和 `_Clock_t` 模板参数提供灵活的时间精度和时钟选择。

    **主要特性**：
    - 支持多种定时器模式（`call_once`、`fixed_rate`、`fixed_delay`）。
    - 提供 `set_timer` 和 `kill_timer` 方法管理定时器生命周期。
    - 集成虚函数 `_On_timer` 用于自定义定时器事件处理。
    - 使用优先级队列确保定时器按时间顺序触发。

    - **_Arg_t**
    - **用途**：指定定时器事件或任务的参数类型，传递给 `_On_timer` 方法或定时器回调。
    - **默认值**：`std::any`，允许存储任意类型参数，支持动态类型处理。
    - **使用场景**：
        - 继承自 `_Message_loop`，用途与 `_Message_loop::_Arg_t` 相同。
        - 常用于定时器传递上下文数据（如计数器引用、配置对象）。
        - 影响 `set_timer` 和 `_On_timer` 的参数传递。

    - **_Result_t**
    - **用途**：指定消息处理的返回值类型，继承自 `_Message_loop`，定时器事件通常不直接使用。
    - **默认值**：`std::any`，允许返回任意类型结果。
    - **使用场景**：
        - 继承自 `_Message_loop`，主要用于消息处理而非定时器。
        - 可指定具体类型以限制消息```

    System: **_Result_t**
    - **用途**：指定消息处理的返回值类型，继承自 `_Message_loop`, 定时器事件通常不直接使用。
    - **默认值**：`std::any`, 允许返回任意类型结果。
    - **使用场景**：
        - 继承自 `_Message_loop`, 主要用于消息处理而非定时器。
        - 可指定具体类型（如 `int`）以限制返回值类型并提高类型安全性。
        - 影响 `send`, `post_await`, 和 `_On_message` 的返回值处理。

    - **_Key_t**
    - **用途**：指定定时器或消息的键类型，用于标识定时器事件或消息，传递给 `_On_timer` 或 `_On_message` 方法。
    - **默认值**：`uintptr_t`, 提供整数类型的标识，适合枚举值或常量。
    - **使用场景**：
        - 继承自 `_Message_loop`, 用途与 `_Message_loop::_Key_t` 相同。
        - 用于标识定时器（如 `CM_TIMER1`）或消息。
        - 影响 `set_timer`, `kill_timer`, 和 `_On_timer` 的键处理。

    - **_Duration_t**
    - **用途**：指定定时器的时间间隔类型，定义定时器的触发精度和时间单位。
    - **默认值**：`std::chrono::nanoseconds`, 提供纳秒级精度，适合高精度定时需求。
    - **使用场景**：
        - 控制定时器的时间间隔（如毫秒、秒）。
        - 可指定其他类型（如 `std::chrono::milliseconds`）以调整精度或简化代码。
        - 影响 `set_timer`, `set_timer_once`, `set_timer_fixed_delay`, 和 `set_timer_fixed_rate` 的时间参数。

    - **_Clock_t**
    - **用途**：指定定时器使用的时钟类型，控制时间计算的时钟源。
    - **默认值**：`std::chrono::steady_clock`, 提供单调递增的时间，适合定时器调度。
    - **使用场景**：
        - 使用 `std::chrono::steady_clock` 确保定时器不受系统时间调整影响。
        - 可指定其他时钟（如 `std::chrono::system_clock`）以支持基于系统时间的定时需求。
        - 影响定时器的触发时间计算和调度精度。
    */
    template <
        typename _Arg_t = std::any,
        typename _Result_t = std::any,
        typename _Key_t = uintptr_t,
        typename _Duration_t = std::chrono::nanoseconds,
        typename _Clock_t = std::chrono::steady_clock
    >
    class _Timer_message_loop
        : public _Message_loop<_Arg_t, _Result_t, _Key_t>
    {
    protected:
#if COMPAT_CODE
        using _TyBase = _Message_loop<_Arg_t, _Result_t, _Key_t>;
        using _TyArg = typename _TyBase::_TyArg;
        using _TyKey = typename _TyBase::_TyKey;
        using _TyMutex = typename _TyBase::_TyMutex;
        using _TyFunctionWithoutResult = typename _TyBase::_TyFunctionWithoutResult;
#else
        using _TyBase = _Message_loop<_Arg_t, _Result_t, _Key_t>;
        using _TyBase::_TyArg;
        using _TyBase::_TyKey;
        using _TyBase::_TyMutex;
        using _TyBase::_TyFunctionWithoutResult;
#endif
    public:
        enum {
            IDX_KEY = 0,
            IDX_OBJECT = 1,
            IDX_DURATION = 2,
            IDX_TIMEPOINT = 3,
            IDX_SCHEDULE_MODE = 4,
        };

        enum class schedule_mode {
            call_once,    // 一次性调度
            fixed_rate,   // 计划触发：上一次计划时刻 + 周期（可能并发）
            fixed_delay,  // 固定延迟：上一次回调结束 + 周期（不并发）
        };

        using _TyDuration = _Duration_t;
        using _TyTimepoint = typename _Clock_t::time_point;

        using _TyTimerObject = std::variant<
            std::monostate,
            _TyArg,
            _TyFunctionWithoutResult
        >;
        using _TyTimer = std::tuple<_TyKey, _TyTimerObject, _TyDuration, _TyTimepoint, schedule_mode>;
        using _TyTimerPtr = std::shared_ptr<_TyTimer>;
    protected:

        /*
        * ### _On_timer
        * - **功能**：虚函数，处理定时器事件，供用户重写以实现自定义定时器逻辑。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递的定时器内容。
        */
        virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept {}
    public:

        /*
        * ### set_timer（带参数）
        * - **功能**：设置定时器，指定触发时间、参数和调度模式，触发时调用 _On_timer。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发时间。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递给 _On_timer，默认为空。
        *   - `mode`: 调度模式（call_once、fixed_rate、fixed_delay），默认为 fixed_delay。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        */
        template < typename __Rep, typename __Period >
        bool set_timer(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            _TyArg&& arg = {},
            schedule_mode mode = schedule_mode::fixed_delay)
        {
            bool _Result = false;
            _TyBase::_Modify([&]
                {
                    auto _Timer_ptr = std::make_shared<_TyTimer>(
                        key,
                        _TyTimerObject{ arg },
                        elapse,
                        _Clock_t::now() + elapse,
                        mode
                    );

                    auto [_Iter, _Inserted] = _My_timers.insert({ key, _Timer_ptr });

                    if (!_Inserted) {
                        return;
                    }

                    _Insert_sorted_timer(_Timer_ptr);

                    _Result = true;
                }
            );

            return _Result;
        }

        /*
        * ### set_timer_once
        * - **功能**：设置一次性定时器，触发一次后自动移除，调用 _On_timer。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发时间。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递给 _On_timer，默认为空。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        */
        template < typename __Rep, typename __Period >
        inline bool set_timer_once(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            _TyArg&& arg = {}) {
            return set_timer(key, elapse, std::move(arg), schedule_mode::call_once);
        }

        /*
        * ### set_timer_fixed_delay
        * - **功能**：设置固定延迟定时器，每次触发后重新计算间隔，调用 _On_timer。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发间隔。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递给 _On_timer，默认为空。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        */
        template < typename __Rep, typename __Period >
        inline bool set_timer_fixed_delay(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            _TyArg&& arg = {}) {
            return set_timer(key, elapse, std::move(arg), schedule_mode::fixed_delay);
        }

        /*
        * ### set_timer_fixed_rate
        * - **功能**：设置固定频率定时器，按固定时间间隔触发，调用 _On_timer。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发频率。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递给 _On_timer，默认为空。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        */
        template < typename __Rep, typename __Period >
        inline bool set_timer_fixed_rate(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            _TyArg&& arg = {}) {
            return set_timer(key, elapse, std::move(arg), schedule_mode::fixed_rate);
        }

        /*
        * ### set_timer（带回调）
        * - **功能**：设置定时器，指定触发时间、回调函数和参数，触发时执行回调。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发时间。
        *   - `mode`: 调度模式（call_once、fixed_rate、fixed_delay）。
        *   - `func`: 可调用对象，定义定时器触发时的逻辑。
        *   - `args`: 可变参数，传递给 func 的参数，需兼容 _Arg_t。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        * - **备注**：如果定时器已存在，则更新其配置信息。
        */
        template < typename __Rep, typename __Period, typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        bool set_timer(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            schedule_mode mode,
            __Function_t&& func, __Args_t&&... args)
        {
            return _TyBase::_Modify([&]
                {
                    auto _Timer_ptr = std::make_shared<_TyTimer>(
                        key,
                        _TyTimerObject{
                            _TyFunctionWithoutResult(
                                std::bind(
                                    std::forward<__Function_t>(func),
                                    std::forward<__Args_t>(args)...
                                )
                            )
                        },
                        elapse,
                        _Clock_t::now() + elapse,
                        mode
                    );

                    auto [_Iter, _Inserted] = _My_timers.insert({ key, _Timer_ptr });

                    if (!_Inserted) {
                        // If the timer already exists, update the pointer
                        _Iter->second = _Timer_ptr;
                    }

                    // Insert the timer into the sorted list
                    _Insert_sorted_timer(_Timer_ptr);
                }
            );
        }

        /*
        * ### set_timer_once
        * - **功能**：设置一次性定时器，触发一次后自动移除，调用 _On_timer。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发时间。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递给 _On_timer，默认为空。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        */
        template < typename __Rep, typename __Period, typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        inline bool set_timer_once(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            __Function_t&& func, __Args_t&&... args) {
            return set_timer(key, elapse, schedule_mode::call_once, func, args...);
        }

        /*
        * ### set_timer_fixed_delay
        * - **功能**：设置固定延迟定时器，每次触发后重新计算间隔，调用 _On_timer。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发间隔。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递给 _On_timer，默认为空。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        */
        template < typename __Rep, typename __Period, typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        inline bool set_timer_fixed_delay(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            __Function_t&& func, __Args_t&&... args) {
            return set_timer(key, elapse, schedule_mode::fixed_delay, func, args...);
        }

        /*
        * ### set_timer_fixed_rate
        * - **功能**：设置固定频率定时器，按固定时间间隔触发，调用 _On_timer。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），区分定时器。
        *   - `elapse`: 时间间隔（类型为 std::chrono::duration），定义触发频率。
        *   - `arg`: 定时器参数（类型为 _TyArg），传递给 _On_timer，默认为空。
        * - **返回值**：`bool`，表示定时器是否成功设置。
        */
        template < typename __Rep, typename __Period, typename __Function_t, typename... __Args_t,
            std::enable_if_t<std::is_void_v<std::invoke_result_t<__Function_t, __Args_t...>>, int> = 0>
        inline bool set_timer_fixed_rate(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            __Function_t&& func, __Args_t&&... args) {
            return set_timer(key, elapse, schedule_mode::fixed_rate, func, args...);
        }

        /*
        * ### kill_timer
        * - **功能**：移除指定定时器，停止其触发。
        * - **参数**：
        *   - `key`: 定时器标识（类型为 _Key_t），指定要移除的定时器。
        * - **返回值**：`bool`，表示定时器是否成功移除。
        */
        bool kill_timer(_TyKey key)
        {
            bool _Result = false;
            _TyBase::_Modify([&]
                {
                    auto _Iter = _My_timers.find(key);
                    if (_Iter == _My_timers.end()) {
                        return;
                    }

                    _My_timers.erase(_Iter);

                    _Result = true;
                }
            );

            return _Result;
        }

        /*
        * ### dispatch
        * - **功能**：重写 _Message_loop 的 dispatch，运行消息循环并处理定时器事件。
        * - **参数**：无。
        */
        virtual void dispatch() noexcept override
        {
            for (;;)
            {
                std::unique_lock<_TyMutex> locker(_TyBase::_My_mtx);
                auto _Timer_ptr = _Pop_front_timer();
                if (!_Timer_ptr) {
                    // Wait for the event

                    _TyBase::_My_cv.wait(
                        locker,
                        [&] {
                            return
                                !_TyBase::_My_items.empty() ||
                                _TyBase::_My_quit_sign ||
                                !_My_sorted_timers.empty();
                        }
                    );

                    if (!_My_sorted_timers.empty()) {
                        continue;
                    }
                }
                else {
                    if (!_TyBase::_My_cv.wait_until(
                        locker,
                        std::get<IDX_TIMEPOINT>(*_Timer_ptr),
                        [&]
                        {
                            return
                                !_TyBase::_My_items.empty() ||
                                _TyBase::_My_quit_sign ||
                                _Removed(_Timer_ptr);
                        }
                    )) {
                        // Timeout
                        assert(!_Removed(_Timer_ptr));

                        auto _Schedule_mode = std::get<IDX_SCHEDULE_MODE>(*_Timer_ptr);
                        if (_Schedule_mode == schedule_mode::fixed_rate) {
                            std::get<IDX_TIMEPOINT>(*_Timer_ptr) += std::get<IDX_DURATION>(*_Timer_ptr);
                            _Insert_sorted_timer(_Timer_ptr);
                        }

                        locker.unlock();
                        _Do_timer(*_Timer_ptr);
                        locker.lock();

                        if (_Schedule_mode == schedule_mode::call_once) {
                            // One-time timer, remove it
                            _My_timers.erase(std::get<IDX_KEY>(*_Timer_ptr));
                        }
                        else if (!_Removed(_Timer_ptr) && // If killed timer in callback
                            _Schedule_mode == schedule_mode::fixed_delay) {
                            std::get<IDX_TIMEPOINT>(*_Timer_ptr) = _Clock_t::now() + std::get<IDX_DURATION>(*_Timer_ptr);
                            _Insert_sorted_timer(_Timer_ptr);
                        }

                        continue;
                    }
                    else {
                        // Event | Quit | Timer removed

                        if (_Removed(_Timer_ptr)) { // If killed timer outside
                            // If timer has been removed, then do nothing
                            continue;
                        }
                        else {
                            // Event, put timer back
                            _Insert_sorted_timer(_Timer_ptr);
                        }
                    }
                }

                // ============================================================
                // Event Start
                if (_TyBase::_My_quit_sign && _TyBase::_My_items.empty()) {
                    // 如果设置了退出标志，并且任务队列为空则退出调度
                    break;
                }

                // Process a event
                auto _Task = std::move(_TyBase::_My_items.front());
                _TyBase::_My_items.pop_front();
                locker.unlock();
                _TyBase::_Do(std::move(_Task));
                // Event End
                // ============================================================
            }
        }
    protected:

        /*
        * ### _Do_timer
        * - **功能**：执行定时器任务，处理定时器对象或回调。
        * - **参数**：
        *   - `_Timer_ref`: 定时器对象（类型为 _TyTimer），包含定时器信息。
        */
        inline void _Do_timer(_TyTimer& _Timer_ref) {
            auto& _Timer_object = std::get<IDX_OBJECT>(_Timer_ref);
            if (auto _Arg = std::get_if<_TyArg>(&_Timer_object)) {
                _On_timer(
                    std::get<IDX_KEY>(_Timer_ref),
                    *_Arg
                );
            }
            else if (auto _Callback = std::get_if<_TyFunctionWithoutResult>(&_Timer_object)) {
                (*_Callback)();
            }
        }

        /*
        * ### _Pop_front_timer
        * - **功能**：从排序的定时器队列中取出最早触发的定时器。
        * - **参数**：无。
        * - **返回值**：`_TyTimerPtr`，指向最早触发的定时器，若队列为空则返回空指针。
        */
        _TyTimerPtr _Pop_front_timer() {
            if (_My_sorted_timers.empty()) return {};

            auto iter = _My_sorted_timers.begin();
            auto _Timer_ptr = iter->second;
            _My_sorted_timers.erase(iter);
            return _Timer_ptr;
        }

        /*
        * ### _Insert_sorted_timer
        * - **功能**：将定时器插入排序的定时器队列，按触发时间排序。
        * - **参数**：
        *   - `_Timer_ptr`: 定时器指针（类型为 _TyTimerPtr），指向要插入的定时器。
        */
        void _Insert_sorted_timer(_TyTimerPtr& _Timer_ptr) {
            _My_sorted_timers.emplace(
                std::get<IDX_TIMEPOINT>(*_Timer_ptr),
                _Timer_ptr
            );
            _TyBase::_My_cv.notify_one();
        }

        /*
        * ### _Removed
        * - **功能**：检查指定定时器是否已被移除。
        * - **参数**：
        *   - `_Timer_ptr`: 定时器指针（类型为 _TyTimerPtr），指向要检查的定时器。
        * - **返回值**：`bool`，若定时器已被移除返回 true，否则返回 false。
        */
        inline bool _Removed(const _TyTimerPtr& _Timer_ptr) {
            auto iter = _My_timers.find(std::get<IDX_KEY>(*_Timer_ptr));
            if (iter == _My_timers.cend()) {
                return true;
            }

            return iter->second != _Timer_ptr;
        }
    protected:
        // Timers
        std::multimap<_TyTimepoint, _TyTimerPtr> _My_sorted_timers;
        std::map<_TyKey, _TyTimerPtr> _My_timers;
    };
    /*
    ## 3. _Thread_loop

    **简介**：
    `_Thread_loop` 是一个单线程消息循环类，将 `_Base_t`（默认 `_Message_loop`）的消息循环运行在独立的后台线程中。它隔离主线程操作，确保任务顺序执行，适合需要线程隔离的场景，如 GUI 事件处理或简单的后台任务调度。

    **主要特性**：
    - 单线程运行消息循环，保证任务串行执行。
    - 提供 `start`、`stop`、`join` 方法管理线程生命周期。
    - 支持自定义 `_Base_t`（如 `_Timer_message_loop`）以扩展功能。
    - 线程安全任务投递，简化跨线程通信。

    - **_Base_t**
    - **用途**：指定基础消息循环类型，定义 `_Thread_loop` 使用的消息循环实现。
    - **默认值**：`message_loop<>`, 使用默认的 `_Message_loop` 配置（`std::any` 参数和返回值，`uintptr_t` 键）。
    - **使用场景**：
        - 默认使用 `_Message_loop` 提供基本消息循环功能。
        - 可指定 `_Timer_message_loop` 或自定义消息循环类型以支持定时器或其他扩展功能。
        - 影响 `_Thread_loop` 的消息处理和任务分发能力。
    */
    template <typename _Base_t = _Message_loop<>>
    class _Thread_loop : public _Base_t
    {
    public:
        virtual ~_Thread_loop() {
            stop();
        }
    public:
        /*
        * ### start
        * - **功能**：启动后台线程运行消息循环，处理任务和消息。
        * - **参数**：无。
        * - **返回值**：`bool`，表示线程是否成功启动。
        */
        bool start() {
            if (_My_threads.joinable()) {
                return false;
            }

            _My_threads = std::thread(
                &_Base_t::dispatch, this
            );

            return true;
        }

        /*
        * ### stop
        * - **功能**：投递退出消息，停止消息循环并等待线程结束，清理资源。
        * - **参数**：
        *   - `_Cancel_pending`: 布尔值，若为 true，清空队列中未处理的任务，默认为 false。
        */
        void stop(bool _Cancel_pending = false) {
            if (joinable()) {
                _Base_t::post_quit_message(_Cancel_pending);
                join();
            }
        }

        /*
        * ### joinable
        * - **功能**：检查线程是否可加入，判断线程运行状态。
        * - **参数**：无。
        * - **返回值**：`bool`，若线程可加入（运行中或已结束但未清理）则返回 true。
        */
        inline bool joinable() const noexcept {
            return _My_threads.joinable();
        }

        /*
        * ### join
        * - **功能**：等待线程结束，确保线程安全退出。
        * - **参数**：无。
        */
        inline void join() {
            return _My_threads.join();
        }
    protected:
        std::thread _My_threads;
    };
    /*
    ## 4. _Thread_pool_loop

    **简介**：
    `_Thread_pool_loop` 是一个线程池消息循环类，利用多个线程并发处理 `_Base_t`（默认 `_Message_loop`）的消息和任务。它支持动态调整线程数量，优化高负载场景的性能，适用于服务器应用或并发任务处理场景。

    **主要特性**：
    - 多线程并发执行任务，提升吞吐量。
    - 提供 `start`、`stop`、`get_thread_count` 方法管理线程池。
    - 支持自定义 `_Base_t`（如 `_Timer_message_loop`）以扩展功能。
    - 自动适配硬件并发性，优化资源利用率。

    - **_Base_t**
    - **用途**：指定基础消息循环类型，定义 `_Thread_pool_loop` 使用的消息循环实现。
    - **默认值**：`message_loop<>`, 使用默认的 `_Message_loop` 配置（`std::any` 参数和返回值，`uintptr_t` 键）。
    - **使用场景**：
        - 默认使用 `_Message_loop` 提供基本消息循环功能。
        - 可指定 `_Timer_message_loop` 或自定义消息循环类型以支持定时器或其他扩展功能。
        - 影响 `_Thread_pool_loop` 的消息处理和并发任务分发能力。
    */
    template <typename _Base_t = _Message_loop<>>
    class _Thread_pool_loop : public _Base_t
    {
    protected:
        using _Base_t::dispatch;
    public:
        virtual ~_Thread_pool_loop() {
            stop();
        }
    public:

        /*
        * ### start
        * - **功能**：启动线程池，创建指定数量的线程运行消息循环。
        * - **参数**：
        *   - `_Thread_count`: 线程数量，默认为硬件并发线程数。
        * - **返回值**：`bool`，表示线程池是否成功启动。
        */
        virtual bool start(std::size_t _Thread_count = std::thread::hardware_concurrency()) noexcept
        {
            if (joinable()) {
                return false;
            }

            _My_threads.clear();
            _My_threads.reserve(_Thread_count);

            for (std::size_t i(0); i < _Thread_count; ++i) {
                _My_threads.emplace_back(&_Base_t::dispatch, this);
            }

            return true;
        }

        /*
         * ### get_thread_count
         * - **功能**：获取线程池中的线程数量。
         * - **参数**：无。
         * - **返回值**：`std::size_t`，当前线程池的线程数。
         */
        std::size_t get_thread_count() const {
            return _My_threads.size();
        }

        /*
        * ### stop
        * - **功能**：投递退出消息，停止线程池并等待所有线程结束，清理资源。
        * - **参数**：
        *   - `_Cancel_pending`: 布尔值，若为 true，清空队列中未处理的任务，默认为 false。
        */
        void stop(bool _Cancel_pending = false) {
            if (joinable()) {
                _Base_t::post_quit_message(_Cancel_pending);
                join();
            }
        }

        /*
        * ### joinable
        * - **功能**：检查是否存在可加入的线程，判断线程池运行状态。
        * - **参数**：无。
        * - **返回值**：`bool`，若存在可加入线程则返回 true。
        */
        inline bool joinable() const noexcept {
            auto _Iter = std::find_if(
                _My_threads.cbegin(),
                _My_threads.cend(),
                [](const auto& _Thread_ref) { return _Thread_ref.joinable(); }
            );

            return _Iter != _My_threads.cend();
        }

        /*
        * ### join
        * - **功能**：等待所有线程结束，确保线程池安全退出。
        * - **参数**：无。
        */
        inline void join() {
            std::for_each(
                _My_threads.begin(),
                _My_threads.end(),
                [](auto& _Thread_ref) {
                    if (_Thread_ref.joinable()) {
                        _Thread_ref.join();
                    }
                }
            );
        }
    protected:
        std::vector<std::thread> _My_threads;
    };
}

namespace msgx {
    // 基础消息循环
    template <
        typename Arg = std::any,
        typename Result = std::any,
        typename Key = uintptr_t
    >
    using message_loop = _Message_loop<Arg, Result, Key>;

    // 定时器支持的消息循环
    template <
        typename Arg = std::any,
        typename Result = std::any,
        typename Key = uintptr_t,
        typename Duration = std::chrono::nanoseconds,
        typename Clock = std::chrono::steady_clock
    >
    using timer_message_loop = _Timer_message_loop<Arg, Result, Key, Duration, Clock>;

    // 线程池版本消息循环
    template <
        typename Base = message_loop<>
    >
    using thread_pool_loop = _Thread_pool_loop<Base>;

    // 单线程后台线程版本消息循环
    template <
        typename Base = message_loop<>
    >
    using thread_loop = _Thread_loop<Base>;

}

// ================================================================================
// ===== 消息映射函数接口宏 =====

#define XDECLARE_ON_MESSAGE() \
    virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept override

#define XDEFINE_ON_MESSAGE(_Class) \
    _Class::_TyResult _Class::_On_message(_TyKey key, _TyArg& arg) noexcept

// ===== 消息映射实现结构宏 =====

#define XBEGIN_MSG_MAP() \
protected: \
    XDECLARE_ON_MESSAGE() {

#define XMESSAGE_HANDLER(msgCode, func) \
    if (key == msgCode) { \
        return func(key, arg); \
    }

#define XMESSAGE_RANGE_HANDLER(msgFirst, msgLast, func) \
    if (key >= msgFirst && key <= msgLast) { \
        return func(key, arg); \
    }

#define XCHAIN_MSG_MAP(_Base_class) \
    { \
        auto _Result = _Base_class::_On_message(key, arg); \
        if (_Result.has_value()) { \
            return _Result; \
        } \
    }

#define XCHAIN_MSG_MAP_MEMBER(_Member) \
    { \
        auto _Result = _Member._On_message(key, arg); \
        if (_Result.has_value()) { \
            return _Result; \
        } \
    }

#define XEND_MSG_MAP_RETURN_BASE(_Base_class) \
    return _Base_class::_On_message(key, arg); \
}

#define XEND_MSG_MAP_RETURN_VALUE(_Result) \
    return _Result; \
}

#define XEND_MSG_MAP() \
    return {}; \
}

// ===== 定时器映射函数接口宏 =====

#define XDECLARE_ON_TIMER() \
    virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override

#define XDEFINE_ON_TIMER(_Class) \
    void _Class::_On_timer(_TyKey key, _TyArg& arg) noexcept

// ===== 定时器映射实现结构宏 =====

#define XBEGIN_TIMER_MAP() \
protected: \
    XDECLARE_ON_TIMER() {

#define XTIMER_HANDLER(timerId, func) \
    if (key == timerId) { \
        func(key, arg); \
        return; \
    }

#define XTIMER_RANGE_HANDLER(timerBegin, timerEnd, func) \
    if (key >= timerBegin && key <= timerEnd) { \
        func(key, arg); \
        return; \
    }

#define XCHAIN_TIMER_MAP(_Base_class) \
    _Base_class::_On_timer(key, arg);

#define XCHAIN_TIMER_MAP_MEMBER(_Member) \
    _Member._On_timer(key, arg);

#define XEND_TIMER_MAP() \
}

// ================================================================================