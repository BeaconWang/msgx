#include <msgx.hpp>

#include <iostream>
#if defined(__cpp_lib_format)
#include <format>
#else
#include <string>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <thread>
#include <type_traits>

namespace std {

    template<typename T>
    std::string to_string_custom(const T& value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        }
        else if constexpr (std::is_same_v<T, const char*>) {
            return std::string(value);
        }
        else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(value);
        }
        else if constexpr (std::is_same_v<T, std::thread::id>) {
            std::ostringstream oss;
            oss << value; // thread::id 支持 operator<<
            return oss.str();
        }
        else if constexpr (std::is_same_v<T, std::string_view>) {
            return std::string(value);  // 将 std::string_view 转换为 std::string
        }
        else {
            static_assert(sizeof(T) == 0, "Unsupported type in format.");
        }
    }

    // Specialization for __FUNCTION__ (function name)
    inline std::string to_string_custom(const char* value) {
        return value ? std::string(value) : "";
    }

    template<typename T, typename... Args>
    void format_recursive(std::ostringstream& oss, const std::string& fmt, size_t& pos, T&& value, Args&&... args) {
        size_t placeholder = fmt.find("{}", pos);
        if (placeholder == std::string::npos) {
            throw std::runtime_error("Too many arguments provided for format string");
        }

        oss << fmt.substr(pos, placeholder - pos);
        oss << to_string_custom(std::forward<T>(value));

        pos = placeholder + 2;

        if constexpr (sizeof...(args) > 0) {
            format_recursive(oss, fmt, pos, std::forward<Args>(args)...);
        }
    }

    inline void format_recursive(std::ostringstream& oss, const std::string& fmt, size_t& pos) {
        oss << fmt.substr(pos);
    }

    template<typename... Args>
    std::string format(const std::string& fmt, Args&&... args) {
        std::ostringstream oss;
        size_t pos = 0;
        format_recursive(oss, fmt, pos, std::forward<Args>(args)...);
        return oss.str();
    }

} // namespace std

#endif
#include <thread>
#include <sstream>
#include <span>
#include <chrono>

using namespace msgx;

#if !COMPAT_CODE
template<>
struct std::formatter<std::thread::id, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const std::thread::id& id, std::format_context& ctx) const {
        std::ostringstream oss;
        oss << id;
        return std::format_to(ctx.out(), "{}", oss.str());
    }
};
#endif

class Tester
{
public:
    template < typename __Rep, typename __Period >
    void test_post_single(const std::chrono::duration<__Rep, __Period> simulate_work_elapse)
    {
        std::cout << std::format("[{}] Enter {}\n", std::this_thread::get_id(), __FUNCTION__);
        _Message_loop<> msgLoop;
        std::thread thread_dispatch(
            &_Message_loop<>::dispatch, &msgLoop
        );

        _Test_post(msgLoop, simulate_work_elapse);

        msgLoop.post_quit_message();
        if (thread_dispatch.joinable()) {
            thread_dispatch.join();
        }
        std::cout << std::format("[{}] Leave {}\n\n", std::this_thread::get_id(), __FUNCTION__);
    }

    template < typename _Rep, typename _Period >
    void test_post_thread_pool(std::size_t thread_count,
        const std::chrono::duration<_Rep, _Period> simulate_work_elapse)
    {
        std::cout << std::format("[{}] Enter {}\n", std::this_thread::get_id(), __FUNCTION__);
        _Thread_pool_loop tpLoop;
        tpLoop.start(thread_count);

        _Test_post(tpLoop, simulate_work_elapse);

        tpLoop.post_quit_message();
        tpLoop.join();
        std::cout << std::format("[{}] Leave {}\n\n", std::this_thread::get_id(), __FUNCTION__);
    }
protected:
    template < typename _Rep, typename _Period >
    void _Test_post(_Message_loop<>& msgLoop,
        const std::chrono::duration<_Rep, _Period> simulate_work_elapse)
    {
        using namespace std::chrono_literals;

        std::atomic_int call_count{};
        // 不关心返回值
        msgLoop.post(
            [&]
            {
                std::cout << std::format("[{}] ==> Post callback 0[Arguments] 0[Return] 0[Wait]\n", std::this_thread::get_id());
                std::this_thread::sleep_for(simulate_work_elapse);
                call_count++;
                std::cout << std::format("[{}] <== Post callback 0[Arguments] 0[Return] 0[Wait]\n", std::this_thread::get_id());
            });
        msgLoop.post(
            [&](int a1, std::string a2, const std::vector<int>& a3)
            {
                std::cout << std::format("[{}] ==> Post callback 3[Arguments] 1[Return] 0[Wait]\n", std::this_thread::get_id());
                std::this_thread::sleep_for(simulate_work_elapse);
                assert(a1 == 0x12345);
                assert(a2.compare("string_value") == 0);
                assert(a3.size() == 4);

                call_count++;
                std::cout << std::format("[{}] <== Post callback 3[Arguments] 1[Return] 0[Wait]\n", std::this_thread::get_id());
                return std::string("discard_return_value");
            },
            0x12345, std::string("string_value"), std::vector<int>{ 1, 2, 3, 4 }
        );

        // 关心返回值
        auto fut1 = msgLoop.post_await(
            [&] {
                std::cout << std::format("[{}] ==> Post callback 0[Arguments] 0[Return] 1[Wait]\n", std::this_thread::get_id());
                std::this_thread::sleep_for(simulate_work_elapse);

                call_count++;
                std::cout << std::format("[{}] <== Post callback 0[Arguments] 0[Return] 1[Wait]\n", std::this_thread::get_id());
            });
        int ref_value = 0x88888888;
        auto fut2 = msgLoop.post_await(
            [&](const std::vector<std::string>& a1, int a2, double a3, std::string a4, const char* a5, int& a6)
            {
                std::cout << std::format("[{}] ==> Post callback 6[Arguments] 1[Return] 1[Wait]\n", std::this_thread::get_id());
                std::this_thread::sleep_for(simulate_work_elapse);

                assert(a1.size() == 3 && a1[1].compare("hello") == 0);
                assert(a2 == 0x44448888);
                assert(a3 > 3.0f);
                assert(a4.compare("hello world") == 0);
                assert(a5 == "raw string"); // address compare
                assert(a6 == 0x88888888);
                a6 = 0x66666666;

                call_count++;
                std::cout << std::format("[{}] <== Post callback 6[Arguments] 1[Return] 1[Wait]\n", std::this_thread::get_id());
                return std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9};
            },
            std::vector<std::string>{"a", "hello", "world"},
            0x44448888, 3.14159, // -> int, double
            "hello world", // -> std::string
            "raw string", // -> const char*
            std::ref(ref_value) // int&
        );

        std::vector<int> v{ 1,2,3,4 };
        std::string s("in_string");
        auto fut3 = msgLoop.post_await_cast(
            [vv = std::move(v)](std::reference_wrapper<std::string> str_ref, std::string_view sv) mutable
            {
                assert(vv.size() == 4);
                vv.push_back(5);

                assert(str_ref.get().compare("in_string") == 0);
                str_ref.get() = "out_string";

                assert(sv.compare("abc") == 0);

                return std::string("abc");
            }, std::ref(s), std::string("abc"));

        std::cout << std::format("[{}] Wait future 1\n", std::this_thread::get_id());
        fut1.wait();
        std::cout << std::format("[{}] Wait future 1 OK\n", std::this_thread::get_id());

        std::cout << std::format("[{}] Wait future 2\n", std::this_thread::get_id());
        auto ar2 = fut2.get();
        assert(ar2.has_value());
        auto r2 = std::any_cast<std::vector<int>>(ar2);
        assert(r2.size() == 9);
        assert(ref_value == 0x66666666);
        assert(fut3.get().compare("abc") == 0);
        assert(s.compare("out_string") == 0);
        std::cout << std::format("[{}] Wait future 2 OK -> check result OK\n", std::this_thread::get_id());

        assert(call_count == 4);
        std::cout << std::format("[{}] Check call count OK\n", std::this_thread::get_id());
    }
public:
    void test_message()
    {
        enum
        {
            CM_MESSAGE0 = 0x12345678,
            CM_MESSAGE1 = 0x23456789,
            CM_MESSAGE2 = 0x22222222,
        };

        class my_class1 : public _Message_loop<>
        {
            virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept override final
            {
                if (key == CM_MESSAGE0) {
                    std::string& str = std::any_cast<std::string&>(arg);
                    assert(str.compare("Hello Beacon") == 0);
                    std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE0 argument from post OK\n", std::this_thread::get_id(), __FUNCTION__);
                }
                else if (key == CM_MESSAGE1) {
                    auto pi = std::any_cast<double&>(arg);
                    assert(pi > 3.1 && pi < 3.2);
                    std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE1 argument from send OK\n", std::this_thread::get_id(), __FUNCTION__);
                }
                else if (key == CM_MESSAGE2) {
                    auto& v_ref = std::any_cast<std::reference_wrapper<std::vector<int>>>(arg).get();
                    assert(v_ref.size() == 6 && v_ref[0] == 1);
                    std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE2 argument from send OK\n", std::this_thread::get_id(), __FUNCTION__);
                    v_ref.push_back(7);
                    v_ref.push_back(8);
                }
                hit_count++;
                std::cout << std::format("[{}] <==> {}\n", std::this_thread::get_id(), __FUNCTION__);
                return 123;
            }

        public:
            std::size_t hit_count{ 0 };
        } m1;
        std::thread th_disp1(&my_class1::dispatch, &m1);

        m1.post(CM_MESSAGE0, std::string("Hello Beacon"));
        auto anyResult = m1.send(CM_MESSAGE1, 3.14159);
        assert(std::any_cast<int>(anyResult) == 123);
        std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE1 return from send OK\n", std::this_thread::get_id(), __FUNCTION__);

        std::vector<int> v{ 1,2,3,4,5,6 };
        // std::any a = std::ref(v);
        m1.send(CM_MESSAGE2, std::ref(v));
        assert(v.size() == 8 && v.at(7) == 8);
        std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE2 reference argument check OK\n", std::this_thread::get_id(), __FUNCTION__);

        m1.post_quit_message();
        if (th_disp1.joinable()) {
            th_disp1.join();
        }
        assert(m1.hit_count == 3);

        ///
        // 测试其它类型参数
        class my_class2 : public _Message_loop<std::string>
        {
            virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept override final
            {
                assert(key == CM_MESSAGE0);
                assert(arg.compare("Hello Beacon") == 0);
                std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE0 argument check OK\n", std::this_thread::get_id(), __FUNCTION__);
                hit_count++;
                return {};
            }
        public:
            std::size_t hit_count{ 0 };
        } m2;
        std::thread th_disp2(
            [&] { m2.dispatch(); }
        );

        m2.post(CM_MESSAGE0, "Hello Beacon"); // 自动转换为目标类型

        m2.post_quit_message();
        if (th_disp2.joinable()) {
            th_disp2.join();
        }
        assert(m2.hit_count == 1);

        /// 
        // 测试传出参数， 引用类型
        class my_class3 : public _Message_loop<std::string&>
        {
            virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept override final
            {
                assert(key == CM_MESSAGE0);
                assert(arg.compare("Hello Beacon") == 0);
                std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE0 argument check OK\n", std::this_thread::get_id(), __FUNCTION__);
                arg = "Result";
                hit_count++;
                return {};
            }
        public:
            std::size_t hit_count{ 0 };
        } m3;
        std::thread th_disp3(
            [&] { m3.dispatch(); }
        );

        std::string sArg("Hello Beacon");
        m3.post(CM_MESSAGE0, sArg);

        m3.post_quit_message();
        if (th_disp3.joinable()) {
            th_disp3.join();
        }

        assert(sArg.compare("Result") == 0);
        std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE0 out reference argument check OK\n", std::this_thread::get_id(), __FUNCTION__);
        assert(m3.hit_count == 1);
    }

    void test_thread_pool()
    {
        using namespace std::chrono_literals;
        _Thread_pool_loop<_Timer_message_loop<>> ttml;
        _Thread_pool_loop<_Message_loop<>> tml;

        ttml.start(std::thread::hardware_concurrency());
        tml.start(std::thread::hardware_concurrency());

        tml.post(
            [] {
                std::cout << std::format("[{}] <==> {}\n", std::this_thread::get_id(), __FUNCTION__);
            }
        );
        auto fut0 = tml.post_await(
            [] {
                std::cout << std::format("[{}] <==> {}\n", std::this_thread::get_id(), __FUNCTION__);
            }
        );
        ttml.post_await( // like post
            [] {
                std::cout << std::format("[{}] <==> {}\n", std::this_thread::get_id(), __FUNCTION__);
                return 1234;
            }
        );
        ttml.set_timer_fixed_rate(1, 500ms, [] {
            std::cout << std::format("[{}] <==> {}\n", std::this_thread::get_id(), __FUNCTION__);
            });
        auto fut1 = ttml.post_await_cast( // like post
            [](const std::vector<int>& v, std::string_view s) {
                assert(v.size() == 4);
                assert(s.compare("my_string") == 0);
                std::cout << std::format("[{}] <==> {} -> {}, {}\n", std::this_thread::get_id(), __FUNCTION__, v.size(), s);
                std::this_thread::sleep_for(1s);
                return 1234;
            },
            std::vector<int>{1, 2, 3, 4}, std::string("my_string")
        );

        fut0.get();

        fut1.wait();
        assert(fut1.get() == 1234);
    }
    void test_send_lambda()
    {
        thread_loop<message_loop<>> m;
        m.start();

        auto s = m.send(
            []
            {
                return std::string("Abcd");
            });
        static_assert(std::is_same_v<decltype(s), std::string>);
        assert(s.compare("Abcd") == 0);

        thread_loop<message_loop<std::any, int>> m1;
        m1.start();

        auto n = m1.send(
            [](std::string_view sv)
            {
                return (short)1234;
            }, "abcd"
        );
        static_assert (std::is_same_v<decltype(n), int>);
        assert(n == 1234);
    }
    void test_macro()
    {
        using namespace std::chrono_literals;
        enum
        {
            CM_MESSAGE0 = 0x12345678,
            CM_MESSAGE1 = 0x23456789,
            CM_MESSAGE2 = 0x22222222,
        };

        class my_class1 : public thread_loop<timer_message_loop<>>
        {
            XBEGIN_MSG_MAP()
                XMESSAGE_HANDLER(CM_MESSAGE0, _On_message0)
                XMESSAGE_HANDLER(CM_MESSAGE1, _On_message1)
                XMESSAGE_HANDLER(CM_MESSAGE2, _On_message2)
                XEND_MSG_MAP()

                _TyResult _On_message0(_TyKey key, _TyArg& arg) noexcept
            {
                std::string& str = std::any_cast<std::string&>(arg);
                assert(str.compare("Hello Beacon") == 0);
                std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE0 argument from post OK\n", std::this_thread::get_id(), __FUNCTION__);
                return 1;
            }
            _TyResult _On_message1(_TyKey key, _TyArg& arg) noexcept
            {
                auto pi = std::any_cast<double&>(arg);
                assert(pi > 3.1 && pi < 3.2);
                std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE1 argument from send OK\n", std::this_thread::get_id(), __FUNCTION__);
                return 2;
            }
            _TyResult _On_message2(_TyKey key, _TyArg& arg) noexcept
            {
                auto& v_ref = std::any_cast<std::reference_wrapper<std::vector<int>>>(arg).get();
                assert(v_ref.size() == 6 && v_ref[0] == 1);
                std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE2 argument from send OK\n", std::this_thread::get_id(), __FUNCTION__);
                v_ref.push_back(7);
                v_ref.push_back(8);
                return 3;
            }

            XBEGIN_TIMER_MAP()
                XTIMER_HANDLER(1, _On_timer1)
                XEND_TIMER_MAP()

                void _On_timer1(_TyKey key, _TyArg& arg) noexcept
            {
                std::cout << std::format("[{}] <==> {}\n", std::this_thread::get_id(), __FUNCTION__);
            }
        } m1;

        m1.start();
        m1.post(CM_MESSAGE0, std::string("Hello Beacon"));

        auto anyResult = m1.send(CM_MESSAGE1, 3.14159);
        assert(std::any_cast<int>(anyResult) == 2);
        std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE1 return from send OK\n", std::this_thread::get_id(), __FUNCTION__);

        std::vector<int> v{ 1,2,3,4,5,6 };
        // std::any a = std::ref(v);
        m1.send(CM_MESSAGE2, std::ref(v));
        assert(v.size() == 8 && v.at(7) == 8);
        std::cout << std::format("[{}] <==> {} -> Check CM_MESSAGE2 reference argument check OK\n", std::this_thread::get_id(), __FUNCTION__);

        m1.set_timer(1, 1s, 1 /* arg */);

        std::this_thread::sleep_for(5s);
    }
    class TimerTester
    {
    public:
        void test_timer_simple()
        {
            _Test_timer_fixed_delay();
            _Test_timer_fixed_rate();
            _Test_timer_once();
            _Test_timer_kill_inside();
        }

        void _Test_timer_fixed_delay() {
            using namespace std::chrono_literals;
            class my_class : public _Thread_loop<_Timer_message_loop<>>
            {
                virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override {
                    hit_count++;
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, key);
                    std::this_thread::sleep_for(500ms);
                }
            public:
                std::size_t hit_count{ 0 };
            } m;

            m.start();

            m.set_timer_fixed_delay(1, 900ms);

            std::this_thread::sleep_for(3s);

            m.stop(); // for hit_count safe

            // 3s = 3000ms / (900ms + 500ms) = 2.x = 2
            // 1400 -> 2800
            assert(m.hit_count == 2);
            std::cout << std::format("[{}] <==> {} -> Passed!!!\n", std::this_thread::get_id(), __FUNCTION__);
        }
        void _Test_timer_fixed_rate() {
            using namespace std::chrono_literals;
            class my_class : public _Thread_loop<_Timer_message_loop<>>
            {
                virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override {
                    hit_count++;
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, key);
                    std::this_thread::sleep_for(500ms);
                }
            public:
                std::size_t hit_count{ 0 };
            } m;

            m.start();

            m.set_timer_fixed_rate(1, 900ms);

            std::this_thread::sleep_for(3s);

            m.stop(); // for hit_count safe

            // 3s = 3000ms / 900ms = 3.x = 3
            // 900 -> 1800 -> 2700
            assert(m.hit_count == 3);
            std::cout << std::format("[{}] <==> {} -> Passed!!!\n", std::this_thread::get_id(), __FUNCTION__);
        }
        void _Test_timer_once() {
            using namespace std::chrono_literals;
            class my_class : public _Thread_loop<_Timer_message_loop<>>
            {
                virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override {
                    hit_count++;
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, key);
                    std::this_thread::sleep_for(200ms);
                }
            public:
                std::size_t hit_count{ 0 };
            } m;

            m.start();

            m.set_timer_once(1, 900ms);
            m.set_timer_once(2, 500ms);
            m.set_timer_once(3, 300ms);
            m.set_timer_once(4, 600ms);
            m.set_timer_once(5, 1000ms);
            m.set_timer_once(6, 2000ms);
            m.set_timer_once(7, 4000ms);
            m.kill_timer(4);

            std::this_thread::sleep_for(3s);

            m.stop(); // for hit_count safe

            // 3 -> 2 -> 1 -> 5 -> 6
            // 4 has been killed
            // 7 > 3s
            assert(m.hit_count == 5);
            std::cout << std::format("[{}] <==> {} -> Passed!!!\n", std::this_thread::get_id(), __FUNCTION__);
        }

        void _Test_timer_kill_inside() {
            using namespace std::chrono_literals;
            class my_class : public _Thread_loop<_Timer_message_loop<>>
            {
                virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override {
                    hit_count++;
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, key);
                    if (hit_count == 10 || key == 1) {
                        kill_timer(key);
                        std::cout << std::format("[{}] <==> {} -> TimerKey: {}, has been killed\n", std::this_thread::get_id(), __FUNCTION__, key);
                    }
                }
            public:
                std::size_t hit_count{ 0 };
            } m;

            m.start();

            m.set_timer_fixed_delay(1, 200ms);
            m.set_timer_fixed_rate(2, 100ms);

            std::this_thread::sleep_for(2s);

            m.stop();

            assert(m.hit_count == 10);
            std::cout << std::format("[{}] <==> {} -> Passed!!!\n", std::this_thread::get_id(), __FUNCTION__);
        }

        void test_timer_thread_pool()
        {
            using namespace std::chrono_literals;
            class my_class : public _Thread_pool_loop<_Timer_message_loop<>>
            {
                virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override {
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, key);
                    auto& ref_hit_count = std::any_cast<std::reference_wrapper<std::atomic<std::size_t>>>(arg).get();
                    ref_hit_count++;

                    if (key == 4 || key == 5)
                    {
                        if (ref_hit_count == 4) {
                            assert(kill_timer(key)); // kill inside 
                        }
                    }

                    std::this_thread::sleep_for(3s);
                }
            } m;

            m.start(32); // Thread count == 32

            std::atomic<std::size_t> hit_count1 = 0;
            m.set_timer_fixed_rate(1, 1000ms, std::ref(hit_count1)); // 20 / 1 = 20
            std::atomic<std::size_t> hit_count2 = 0;
            m.set_timer_fixed_delay(2, 1000ms, std::ref(hit_count2)); // 20 / 4 = 5
            std::atomic<std::size_t> hit_count3 = 0;
            m.set_timer_once(3, 5678ms, std::ref(hit_count3)); // -> 1
            std::atomic<std::size_t> hit_count4 = 0;
            m.set_timer_fixed_rate(4, 300ms, std::ref(hit_count4)); // -> 4
            std::atomic<std::size_t> hit_count5 = 0;
            m.set_timer_fixed_delay(5, 600ms, std::ref(hit_count5)); // -> 4

            // lambda
            m.set_timer_once(6, 7200ms,
                [](int i, char c, std::string_view s)
                {
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}, {}, {}, {}\n", std::this_thread::get_id(), __FUNCTION__, 6, i, c, s);
                }, 123, 'c', std::string("String argument"));
            std::atomic<std::size_t> hit_count7 = 0;
            m.set_timer_fixed_delay(7, 2s,
                [&hit_count7]
                {
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, 7);
                    hit_count7++;
                    std::this_thread::sleep_for(3s);
                }); // 20 / 5 = 4
            std::atomic<std::size_t> hit_count8 = 0;
            m.set_timer_fixed_rate(8, 2s,
                [&hit_count8]
                {
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, 8);
                    hit_count8++;
                    std::this_thread::sleep_for(3s);
                }); // 20 / 2 = 10
            std::atomic<std::size_t> hit_count9 = 0;
            m.set_timer_fixed_rate(9, 1s,
                [&m](std::reference_wrapper<std::atomic<std::size_t>> hit_count_ref)
                {
                    std::cout << std::format("[{}] <==> {} -> TimerKey: {}\n", std::this_thread::get_id(), __FUNCTION__, 9);
                    hit_count_ref.get()++;

                    if (hit_count_ref.get() == 5) {
                        assert(m.kill_timer(9));
                    }
                    std::this_thread::sleep_for(3s);
                }, std::ref(hit_count9));

            std::this_thread::sleep_for(20s + 500ms);
            assert(hit_count1 == 20);
            assert(hit_count2 == 5);
            assert(hit_count3 == 1);
            assert(hit_count4 == 4);
            assert(hit_count5 == 4);
            assert(m.kill_timer(2));
            assert(hit_count7 == 4);
            assert(hit_count8 == 10);
            assert(hit_count9 == 5);
            std::cout << std::format("[{}] <==> {} -> Passed!!!\n", std::this_thread::get_id(), __FUNCTION__);
        }
    };

    void test_timer_all()
    {
        TimerTester tt;
        tt.test_timer_simple();
        tt.test_timer_thread_pool();
    }

    void test_send_timeout()
    {
        // 用于快速测试的线程池
        class MyThreadPool : public msgx::thread_pool_loop<msgx::message_loop<>>
        {
        };

        MyThreadPool mtp;
        mtp.start(); // 启用一堆线程

        class MyMsger : public msgx::thread_pool_loop<msgx::timer_message_loop<>>
        {
        private:
            virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return {};
            }
        };

        MyMsger m;
        m.start();

        // 测试发送消息超时接口
        mtp.post(
            [&]
            {
                // 相对时间
                auto r1 = m.send(std::chrono::milliseconds(500), 1, "string-arg");
                assert(!r1.has_value()); // Timeout
                auto r2 = m.send(std::chrono::seconds(2), 1, "string-arg");
                assert(r2.has_value()); // Timeout

                // 绝对时间
                auto r3 = m.send(std::chrono::steady_clock::now() + std::chrono::milliseconds(500), 1, "string-arg");
                assert(!r3.has_value()); // Timeout
                auto r4 = m.send(std::chrono::steady_clock::now() + std::chrono::seconds(2), 1, "string-arg");
                assert(r4.has_value()); // Timeout
            }
        );

        // 测试 send lambda 表达式超时接口
        mtp.post(
            [&]
            {
                using _Result_t = int;
                msgx::thread_loop<msgx::message_loop<std::any, _Result_t>> tlm;
                tlm.start();

                // 相对时间
                bool r1 = tlm.send(
                    std::chrono::milliseconds(500),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    },
                    'A', 100
                );
                assert(!r1);
                bool r2 = tlm.send(
                    std::chrono::milliseconds(2000),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    },
                    'A', 100
                );
                assert(r2);

                // 绝对时间
                bool r3 = tlm.send(
                    std::chrono::system_clock::now() + std::chrono::milliseconds(500),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    },
                    'A', 100
                );
                assert(!r3);
                bool r4 = tlm.send(
                    std::chrono::system_clock::now() + std::chrono::milliseconds(2000),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    },
                    'A', 100
                );
                assert(r4);
            }
        );

        // 测试 send lambda 表达式超时接口
        mtp.post(
            [&]
            {
                using _Result_t = int;
                msgx::thread_loop<msgx::message_loop<std::any, _Result_t>> tlm;
                tlm.start();
                // 相对时间
                std::optional<_Result_t> r1 = tlm.send(
                    std::chrono::milliseconds(500),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return 1;
                    },
                    'A', 100
                );
                assert(!r1.has_value());
                std::optional<_Result_t> r2 = tlm.send(
                    std::chrono::milliseconds(2000),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return 1;
                    },
                    'A', 100
                );
                assert(r2.has_value());
                // 绝对时间
                std::optional<_Result_t> r3 = tlm.send(
                    std::chrono::system_clock::now() + std::chrono::milliseconds(500),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return 1;
                    },
                    'A', 100
                );
                assert(!r3.has_value());
                std::optional<_Result_t> r4 = tlm.send(
                    std::chrono::system_clock::now() + std::chrono::milliseconds(2000),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return 1;
                    },
                    'A', 100
                );
                assert(r4.has_value());
            }
        );
        // 测试 send lambda 表达式超时接口
        mtp.post(
            [&]
            {
                using _Result_t = const char*;
                msgx::thread_loop<msgx::message_loop<>> tlm;
                tlm.start();
                // 相对时间
                std::optional<_Result_t> r1 = tlm.send(
                    std::chrono::milliseconds(500),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return "ReplyString";
                    },
                    'A', 100
                );
                assert(!r1.has_value());
                std::optional<_Result_t> r2 = tlm.send(
                    std::chrono::milliseconds(2000),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return "ReplyString";
                    },
                    'A', 100
                );
                assert(r2.has_value());
                // 绝对时间
                std::optional<_Result_t> r3 = tlm.send(
                    std::chrono::system_clock::now() + std::chrono::milliseconds(500),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return "ReplyString";
                    },
                    'A', 100
                );
                assert(!r3.has_value());
                std::optional<_Result_t> r4 = tlm.send(
                    std::chrono::system_clock::now() + std::chrono::milliseconds(2000),
                    [](char c, int i)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        return "ReplyString";
                    },
                    'A', 100
                );
                assert(r4.has_value());
            }
        );
        mtp.stop();
    }

    void test_disabled()
    {
        class MyMsger : public msgx::thread_pool_loop<msgx::timer_message_loop<>>
        {
        private:
            virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                assert(disabled());
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return {};
            }
        };

        MyMsger m;
        m.start();
        m.post(1, {});
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        m.stop();

        /// 

        msgx::thread_loop<msgx::message_loop<>> tlm;
        tlm.start();

        tlm.post(
            [&]
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                assert(tlm.disabled());
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        );

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        m.stop();
    }
};

int main()
{
    Tester tester;
    tester.test_send_lambda();
    tester.test_post_single(std::chrono::seconds(1));
    tester.test_post_thread_pool(10, std::chrono::seconds(1));
    tester.test_message();
    tester.test_thread_pool();
    tester.test_macro();
    tester.test_timer_all();
    tester.test_send_timeout();
    tester.test_disabled();

    return 0;

}