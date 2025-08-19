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
Linux ������Ҫ���ӵĿ⣺
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
/// ��ƽ̨��Ϣѭ��ϵͳ & �̳߳�
///     ������C++STL
///     ԭ������message_loop_core
///     -- Create.Beacon.20220906
///     -- Modify.Beacon.20220920
///         -- 1. �������ϢID��Ͷ���뷢�ͺ���
///         -- 2. �����Ϣ�����
///         -- 3. ��ǿ����
///         -- 4. ������Dispatch�Ĵ��������ǰ����
///         -- 5. ������ǰ�̵߳�Dispatch����
///     -- Modify.Beacon.20230706
///         -- 1. ���������߼�
///     -- Modify.Beacon.20230902
///         -- 1. ʹ��steady_clock������ϵͳ�޸�ʱ��������ʱ����
///     -- Modify.Beacon.20240514
///         -- 1. ��ӵ��û���ĺ�֧��
///     -- Modify.Beacon.20240912
///         -- 1. ��� post_callback_future���ڶ���β��Ͷ��һ���ص�������Future�����κ�λ�õȴ���ִ�����
///         -- 2. ��� post_callback���ڶ���β��Ͷ��һ���ص�����������ʲôʱ��ִ�����
///         -- 3. ��� send_callback���ڶ���ͷ����һ���ص��������ȴ���ִ����Ϻ󷵻�����
///     -- Modify.Beacon.20241022
///         -- 1. �����ڲ�ģ���������ƣ�Ӧ��ĳЩ�м��������������⣩
///     -- Modify.Beacon.20241121
///         -- 1. ��� _Key_t ģ�������ʹ��ϢID�Ͷ�ʱ��ID�����Ϳ����Զ���
///         -- 2. �� _Arg_t ��������Ϊ��ֵ����
///         -- 3. ��ʱ����������Ĳ���
///     -- Modify.Beacon.20241122
///         -- 1. ����ʱ���ṹʹ�� std::shared_ptr ά���������������ڣ�ʹ kill_timer ��һ���Զ�ʱ����ʹ��lambdaʱ�������
///     -- Modify.Beacon.20250328
///         -- 1. ��Ӷ��߳�������֧�� task_manager_core
///         -- 2. ȥ�� send_callback ��ʱ�����е�һ��û��Ҫ��ģ�����
///     -- Modify.Beacon.20250421-20250427
///         -- 1. �ش��ع�
///     -- Modify.Beacon.20250509
///         -- 1. �޸�BUG
///     -- Modify.Beacon.20250603
///         -- 1. �޸�BUG����ĳ��IDֵ��Timer���ڵ�ʱ��kill_timer������set_timer��ͬIDֵ���ᵼ��ǰһ��Timer��������
///     -- Modify.Beacon.20250616
///         -- 1. ������ send ������ӳ�ʱ����
///         -- 2. ��� disabled �����������ж��Ƿ��Ѿ������˳�״̬�����˳������ã�
///     -- Modify.Beacon.20250811
///         -- 1. ����һ���Զ�ʱ�����ڴ�����Ϻ������Ƴ��������ǵȵ��´ε���ʱ���Ƴ�
///         -- 2. ���ڶ��������ͬID�Ķ�ʱ����ԭ�߼��᷵��ʧ�ܣ����߼�ֱ���滻��ʱ��������Ϣ���� Win32API SetTimer ����һ�£�

/*
��Ҫ����
����֧�֣�ʹ�� std::any�����û��Զ������ͣ���Ϊ������_TyArg��������ֵ��_TyResult���ͼ���_TyKey�����ṩ������Ϣ���ݻ��ơ�
�첽��ͬ��������֧���첽�� post ����������������ͬ���� send ������������ִ������
��ʱ��֧�֣��ṩ���ڶ�ʱ���ĵ��ȣ�֧��һ���ԣ�call_once�����̶�Ƶ�ʣ�fixed_rate���͹̶��ӳ٣�fixed_delay��ģʽ��
�߳�ģ�ͣ��������̣߳�_Thread_loop�����̳߳أ�_Thread_pool_loop��ʵ�֣�֧�ֲ���������
��Ϣӳ�䣺�ṩ�궨����Ϣ�Ͷ�ʱ��������������� MFC/Win32 ��Ϣӳ�䣬���¼������߼���
�̰߳�ȫ��ͨ�� std::mutex �� std::condition_variable ȷ����Ϣ���кͶ�ʱ�����̰߳�ȫ��
�쳣����send ����֧���쳣������post_await �� post_await_cast �ɲ�������ִ���е��쳣��
�����������ͣ�֧�� std::function��std::packaged_task ���Զ�����Ϣ�������������������

_Message_loop �ӿ�
post���޷���ֵ���񣩣��첽Ͷ���޷���ֵ����֧������ɵ��ö���Ͳ�����
post���з���ֵ���񣩣��첽Ͷ���з���ֵ���񣬷���ֵ��ת��Ϊ _TyResult��
post��Ͷ����Ϣ�����첽Ͷ�ݼ�ֵ����Ϣ������ _On_message ����
post_await���޷���ֵ�����첽Ͷ���޷���ֵ���񣬷��� std::future<void> �Եȴ���ɡ�
post_await���з���ֵ�����첽Ͷ���з���ֵ���񣬷��� std::future<_TyResult> ��ȡ�����
post_await_cast���첽Ͷ�����񣬷��ؾ�ȷ���� std::future���� _TyResult Ϊ std::any����
send���޷���ֵ����ͬ��Ͷ���޷���ֵ��������ֱ��������ɡ�
send���з���ֵ���� std::any����ͬ��Ͷ���з���ֵ���񣬷��� _TyResult ���ͽ����
send���з���ֵ��std::any����ͬ��Ͷ���з���ֵ���񣬷��ؾ�ȷ���͵� _TyResult��
send��������Ϣ����ͬ�����ͼ�ֵ����Ϣ������ _On_message �Ľ����
post_quit_message��Ͷ���˳���Ϣ��ֹͣ��Ϣѭ����֧�ֿ�ѡȡ��δ��������
dispatch��������Ϣѭ������������е��������Ϣ��������ʵ�֡�
_On_message���麯����������Ϣ�ص���֧���û��Զ�����Ϣ�����߼���
Zero-copy �������ݣ����� std::any ���ƶ�������ٲ�������������
�쳣������send �� post_await ����֧�ֲ���ʹ�������ִ���е��쳣��
����������Ͷ�ݣ���������Ϣ������Ƕ��Ͷ�����������Ϣ��

_Timer_message_loop �ӿ�
set_timer���������������ö�ʱ����ָ������ʱ�䡢�����͵���ģʽ��һ���ԡ��̶�Ƶ�ʡ��̶��ӳ٣���
set_timer�����ص��������ö�ʱ����ָ���ص������Ͳ�����֧�ָ��������߼���
set_timer_once������һ���Զ�ʱ�������δ������Զ��Ƴ���
set_timer_fixed_delay�����ù̶��ӳٶ�ʱ����ÿ�δ��������¼�������
set_timer_fixed_rate�����ù̶�Ƶ�ʶ�ʱ�������̶�ʱ����������
kill_timer���Ƴ�ָ����ʱ����֧�ֶ�̬����ʱ���������ڡ�
dispatch����д _Message_loop �� dispatch�����ɶ�ʱ�����Ⱥ���Ϣ����
_On_timer���麯��������ʱ���¼��ص���֧���û��Զ��嶨ʱ���߼���
��ʱ�����ȼ������ڲ�ʹ�����ȼ�����ȷ����ʱ����ʱ��˳�򴥷���
��������ʱ�����ȣ���ʱ����������������Ϣѭ������֤ʵʱ�ԡ�
�Զ���ʱ��֧�֣�ͨ�� _Clock_t ģ�����֧���û�ָ��ʱ�����͡�
�����ö�ʱ�����ȣ�ͨ�� _Duration_t ģ�����֧�ֲ�ͬʱ�侫�ȡ�

_Thread_loop �ӿ�
start���������߳�������Ϣѭ�����������̲߳�����
stop��ֹͣ��Ϣѭ�����ȴ��߳̽�����������Դ��
joinable������߳��Ƿ�ɼ��룬�ж�����״̬��
join���ȴ��߳̽�����ȷ���̰߳�ȫ�˳���
�̰߳�ȫ����ַ������߳�ģ��ȷ������˳��ִ�У��޲���������

_Thread_pool_loop �ӿ�
start�������̳߳أ�ָ���߳��������Զ�����Ӳ�������ԡ�
stop��ֹͣ�̳߳ز��ȴ������߳̽�����������Դ��
get_thread_count����ȡ��ǰ�̳߳ص��߳�����֧������ʱ��ѯ��
joinable������Ƿ���ڿɼ�����̣߳��ж�����״̬��
join���ȴ������߳̽�����ȷ���̳߳ذ�ȫ�˳���
�̳߳ض�̬����������Ӳ���������Ż��߳�������������Դ�����ʡ�
����������֧�ֶ��̲߳���ִ�������Ż��߸��س�����

��Ϣӳ���
XBEGIN_MSG_MAP����ʼ��Ϣӳ�䶨�壬��ʼ����Ϣ�����߼���
XMESSAGE_HANDLER��Ϊ�ض���Ϣ���󶨴�������֧�ֵ�һ��Ϣ����
XMESSAGE_RANGE_HANDLER��Ϊ��Ϣ����Χ�󶨴�������֧��������Ϣ����
XCHAIN_MSG_MAP�����û������Ϣ����֧�ּ̳�������
XCHAIN_MSG_MAP_MEMBER�����ó�Ա�������Ϣ����֧�����ģʽ��
XEND_MSG_MAP��������Ϣӳ�䣬����Ĭ��ֵ����ɴ����߼���
XEND_MSG_MAP_RETURN_BASE��������Ϣӳ�䣬���ػ��ദ������
XEND_MSG_MAP_RETURN_VALUE��������Ϣӳ�䣬�����û�ָ��ֵ��

��ʱ��ӳ���
XBEGIN_TIMER_MAP����ʼ��ʱ��ӳ�䶨�壬��ʼ����ʱ�������߼���
XTIMER_HANDLER��Ϊ�ض���ʱ�����󶨴�������֧�ֵ�һ��ʱ������
XTIMER_RANGE_HANDLER��Ϊ��ʱ������Χ�󶨴�������֧��������ʱ������
XCHAIN_TIMER_MAP�����û���Ķ�ʱ������֧�ּ̳�������
XCHAIN_TIMER_MAP_MEMBER�����ó�Ա����Ķ�ʱ������֧�����ģʽ��
XEND_TIMER_MAP��������ʱ��ӳ�䣬��ɶ�ʱ�������߼���

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

    **���**��
    `_Message_loop` �� `msgx` ��ĺ����࣬�ṩ��������Ϣѭ�����ƣ������첽��ͬ������ַ�����֧��ͨ����ֵ����Ϣ��ɵ��ö���Ͷ�������������¼�������̡���ͨ������ģ�壨`_Arg_t`��`_Result_t`��`_Key_t`��ʵ�����Ĳ����ͷ���ֵ���������̰߳�ȫ���У��ʺϹ����������¼������ܡ�

    **��Ҫ����**��
    - �첽Ͷ�ݣ�`post`����ͬ��ִ�У�`send`���������Ϣ��
    - ֧�� `std::any` ���Զ������͵Ĳ����ͷ���ֵ��
    - �ṩ�麯�� `_On_message` �����Զ�����Ϣ����
    - ��Ч��������й�����̰߳�ȫ������

    - **_Arg_t**
    - **��;**��ָ����Ϣ������Ĳ������ͣ����ݸ�����ص��� `_On_message` ������
    - **Ĭ��ֵ**��`std::any`������洢�������Ͳ�����֧�ֶ�̬���ʹ���
    - **ʹ�ó���**��
        - ʹ�� `std::any` ��������Ҫ���ݲ�ͬ���Ͳ����ĳ��������ַ������������Զ���ṹ�壩��
        - ��ָ���������ͣ��� `std::string`�������Ʋ������Ͳ�������Ͱ�ȫ�ԡ�
        - Ӱ�� `post`��`send` �� `_On_message` �Ĳ������ݷ�ʽ��

    - **_Result_t**
    - **��;**��ָ���������Ϣ����ķ���ֵ���ͣ����ظ������߻� `_On_message` ������
    - **Ĭ��ֵ**��`std::any`���������������ͽ����֧�ֶ�̬���ʹ���
    - **ʹ�ó���**��
        - ʹ�� `std::any` ��������Ҫ���ز�ͬ���ͽ���ĳ��������������ַ�������������
        - ��ָ���������ͣ��� `int`�������Ʒ���ֵ���Ͳ�������Ͱ�ȫ�ԡ�
        - Ӱ�� `send`��`post_await` �� `_On_message` �ķ���ֵ����

    - **_Key_t**
    - **��;**��ָ����Ϣ�ļ����ͣ����ڱ�ʶ��Ϣ�����񣬴��ݸ� `_On_message` ������
    - **Ĭ��ֵ**��`uintptr_t`���ṩ�������͵���Ϣ��ʶ���ʺ�ö��ֵ������
    - **ʹ�ó���**��
        - ʹ�� `uintptr_t` �����ڼ���Ϣ��ʶ����ö��ֵ `CM_MESSAGE0`����
        - ��ָ���������ͣ��� `std::string`����֧�ָ����ӵ���Ϣ�������ַ�����ʶ����
        - Ӱ�� `post`��`send` �� `_On_message` ����Ϣ������
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
        * - **����**���麯��������Ͷ�ݵ���Ϣ�����û���д��ʵ���Զ�����Ϣ�߼���
        * - **����**��
        *   - `key`: ��Ϣ��ʶ������Ϊ _Key_t����������Ϣ���͡�
        *   - `arg`: ��Ϣ����������Ϊ _TyArg�������ݵ���Ϣ���ݡ�
        * - **����ֵ**��`_TyResult`����Ϣ����Ľ����
        */
        virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept { return {}; }
    public:
        virtual ~_Message_loop() = default;
    public:
        /*
        * ### post���޷���ֵ����
        * - **����**���첽Ͷ��һ���޷���ֵ��������Ϣ���У������� dispatch ����ʱִ�С�
        * - **����**��
        *   - `func`: �ɵ��ö����纯����lambda�������������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`bool`����ʾ�����Ƿ�ɹ�Ͷ�ݵ����С�
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
        * ### post���з���ֵ����
        * - **����**���첽Ͷ��һ���з���ֵ��������Ϣ���У������� dispatch ����ʱִ�С�
        * - **����**��
        *   - `func`: �ɵ��ö��󣬶��������߼�������ֵ����� _TyResult��
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`bool`����ʾ�����Ƿ�ɹ�Ͷ�ݵ����С�
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
        * ### post_await���޷���ֵ��
        * - **����**���첽Ͷ��һ���޷���ֵ�����񣬲����� std::future �Եȴ�������ɡ�
        * - **����**��
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::future<void>`�����ڵȴ�������ɻ򲶻��쳣��
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
        * ### post_await���з���ֵ��
        * - **����**���첽Ͷ��һ���з���ֵ�����񣬲����� std::future �Ի�ȡ�����
        * - **����**��
        *   - `func`: �ɵ��ö��󣬶��������߼�������ֵ����� _TyResult��
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::future<_TyResult>`�����ڻ�ȡ�������򲶻��쳣��
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
        * - **����**���첽Ͷ�����񲢷��ؾ�ȷ���͵� std::future���� _TyResult Ϊ std::any����
        * - **����**��
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::future<std::invoke_result_t<__Function_t, __Args_t...>>`���ṩ��ȷ�����������͡�
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
        * ### post��Ͷ����Ϣ��
        * - **����**���첽Ͷ��һ����ֵ����Ϣ����Ϣ���У����� _On_message ����
        * - **����**��
        *   - `_Key`: ��Ϣ��ʶ������Ϊ _Key_t��������������Ϣ���͡�
        *   - `_Arg`: ��Ϣ����������Ϊ _TyArg�������ݸ� _On_message��
        * - **����ֵ**��`bool`����ʾ��Ϣ�Ƿ�ɹ�Ͷ�ݵ����С�
        */
        bool post(_Key_t _Key, _Arg_t&& _Arg) {
            return _Push_back(_TyMessageWithoutResult{ _Key, _Arg });
        }
    public:

        /*
        * ### send���޷���ֵ��
        * - **����**��ͬ��ִ��һ���޷���ֵ����������ֱ��������ɡ�
        * - **����**��
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
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
        * ### send���޷���ֵ�����ʱ�䳬ʱ��
        * - **����**��ͬ��ִ��һ���޷���ֵ�����񣬴����ʱ�䳬ʱ������ֱ��������ɻ�ʱ��
        * - **����**��
        *   - `_Rel_time`: ���ʱ�䳬ʱ������Ϊ std::chrono::duration����ָ���ȴ�ʱ�䡣
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`bool`���������ڳ�ʱʱ������ɷ��� true�����򷵻� false��
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
        * ### send���޷���ֵ������ʱ�䳬ʱ��
        * - **����**��ͬ��ִ��һ���޷���ֵ�����񣬴�����ʱ�䳬ʱ������ֱ��������ɻ�ʱ��
        * - **����**��
        *   - `_Abs_time`: ����ʱ�䳬ʱ������Ϊ std::chrono::time_point����ָ����ֹʱ�䡣
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`bool`���������ڽ�ֹʱ��ǰ��ɷ��� true�����򷵻� false��
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
        * ### send���з���ֵ���� std::any��
        * - **����**��ͬ��ִ��һ���з���ֵ����������ֱ��������ɡ�
        * - **����**��
        *   - `func`: �ɵ��ö��󣬶��������߼�������ֵ����� _TyResult��
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`__Result_t`������ķ���ֵ��
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
        * ### send���з���ֵ���� std::any�����ʱ�䳬ʱ��
        * - **����**��ͬ��ִ��һ���з���ֵ�����񣬴����ʱ�䳬ʱ������ֱ��������ɻ�ʱ��
        * - **����**��
        *   - `_Rel_time`: ���ʱ�䳬ʱ������Ϊ std::chrono::duration����ָ���ȴ�ʱ�䡣
        *   - `func`: �ɵ��ö��󣬶��������߼�������ֵ����� _TyResult��
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::optional<__Result_t>`����������ɷ��ؽ�������򷵻� std::nullopt��
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
        * ### send���з���ֵ���� std::any������ʱ�䳬ʱ��
        * - **����**��ͬ��ִ��һ���з���ֵ�����񣬴�����ʱ�䳬ʱ������ֱ��������ɻ�ʱ��
        * - **����**��
        *   - `_Abs_time`: ����ʱ�䳬ʱ������Ϊ std::chrono::time_point����ָ����ֹʱ�䡣
        *   - `func`: �ɵ��ö��󣬶��������߼�������ֵ����� _TyResult��
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::optional<__Result_t>`����������ɷ��ؽ�������򷵻� std::nullopt��
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
        * ### send���з���ֵ��std::any��
        * - **����**��ͬ��ִ��һ���з���ֵ�����񣨵� _TyResult Ϊ std::any��������ֱ����ɡ�
        * - **����**��
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::invoke_result_t<__Function_t, __Args_t...>`������ľ�ȷ����ֵ���͡�
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
        * ### send���з���ֵ��std::any�����ʱ�䳬ʱ��
        * - **����**��ͬ��ִ��һ���з���ֵ�����񣨵� _TyResult Ϊ std::any���������ʱ�䳬ʱ������ֱ����ɻ�ʱ��
        * - **����**��
        *   - `_Rel_time`: ���ʱ�䳬ʱ������Ϊ std::chrono::duration����ָ���ȴ�ʱ�䡣
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::optional<std::invoke_result_t<__Function_t, __Args_t...>>`����������ɷ��ؽ�������򷵻� std::nullopt��
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
        * ### send���з���ֵ��std::any������ʱ�䳬ʱ��
        * - **����**��ͬ��ִ��һ���з���ֵ�����񣨵� _TyResult Ϊ std::any����������ʱ�䳬ʱ������ֱ����ɻ�ʱ��
        * - **����**��
        *   - `_Abs_time`: ����ʱ�䳬ʱ������Ϊ std::chrono::time_point����ָ����ֹʱ�䡣
        *   - `func`: �ɵ��ö��󣬶��������߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`std::optional<std::invoke_result_t<__Function_t, __Args_t...>>`����������ɷ��ؽ�������򷵻� std::nullopt��
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
        * ### send��������Ϣ��
        * - **����**��ͬ������һ����ֵ����Ϣ������ֱ�� _On_message ������ɡ�
        * - **����**��
        *   - `_Key`: ��Ϣ��ʶ������Ϊ _Key_t��������������Ϣ���͡�
        *   - `_Arg`: ��Ϣ����������Ϊ _TyArg�������ݸ� _On_message��
        * - **����ֵ**��`_TyResult`��_On_message ����Ľ����
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
        * ### send��������Ϣ�����ʱ�䳬ʱ��
        * - **����**��ͬ������һ����ֵ����Ϣ�������ʱ�䳬ʱ������ֱ�� _On_message ������ɻ�ʱ��
        * - **����**��
        *   - `_Rel_time`: ���ʱ�䳬ʱ������Ϊ std::chrono::duration����ָ���ȴ�ʱ�䡣
        *   - `_Key`: ��Ϣ��ʶ������Ϊ _Key_t��������������Ϣ���͡�
        *   - `_Arg`: ��Ϣ����������Ϊ _TyArg�������ݸ� _On_message��
        * - **����ֵ**��`std::optional<_TyResult>`����������ɷ��� _On_message �Ľ�������򷵻� std::nullopt��
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
        * ### send��������Ϣ������ʱ�䳬ʱ��
        * - **����**��ͬ������һ����ֵ����Ϣ��������ʱ�䳬ʱ������ֱ�� _On_message ������ɻ�ʱ��
        * - **����**��
        *   - `_Abs_time`: ����ʱ�䳬ʱ������Ϊ std::chrono::time_point����ָ����ֹʱ�䡣
        *   - `_Key`: ��Ϣ��ʶ������Ϊ _Key_t��������������Ϣ���͡�
        *   - `_Arg`: ��Ϣ����������Ϊ _TyArg�������ݸ� _On_message��
        * - **����ֵ**��`std::optional<_TyResult>`����������ɷ��� _On_message �Ľ�������򷵻� std::nullopt��
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
        * - **����**��Ͷ���˳���Ϣ��ֹͣ��Ϣѭ������ѡ��ȡ��δ��������
        * - **����**��
        *   - `_Cancel_pending`: ����ֵ����Ϊ true����ն�����δ���������Ĭ��Ϊ false��
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
        * - **����**�������Ϣѭ���Ƿ��ڽ���״̬�����˳��������˳�����
        * - **����**���ޡ�
        * - **����ֵ**��`bool`������Ϣѭ���ѽ��ã��˳���־�����ã��򷵻� true��
        */
        bool disabled() const noexcept {
            std::scoped_lock<_TyMutex> locker(_My_mtx);
            return _My_quit_sign;
        }
    protected:

        /*
        * ### _Modify
        * - **����**���̰߳�ȫ���޸���Ϣ���У�ִ��ָ�������Ͳ�����
        * - **����**��
        *   - `_Modify_function`: Ҫִ�е��޸Ĳ�����lambda �������󣩡�
        * - **����ֵ**��`bool`���������ɹ���δ�����˳�״̬������ true�����򷵻� false��
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
        * - **����**�����������Ϣ��ӵ�����β����
        * - **����**��
        *   - `task`: Ҫ��ӵ��������Ϣ������Ϊ _TyItem����
        * - **����ֵ**��`bool`�����ɹ���ӵ����з��� true�����򷵻� false��
        */
        bool _Push_back(_TyItem&& task) {
            return _Modify([&] { _My_items.push_back(std::move(task)); });
        }

        /*
        * ### _Push_front
        * - **����**�����������Ϣ��ӵ�����ͷ��������ִ�С�
        * - **����**��
        *   - `task`: Ҫ��ӵ��������Ϣ������Ϊ _TyItem����
        * - **����ֵ**��`bool`�����ɹ���ӵ����з��� true�����򷵻� false��
        */
        bool _Push_front(_TyItem&& task) {
            return _Modify([&] { _My_items.push_front(std::move(task)); });
        }

        /*
        * ### _Do
        * - **����**��ִ�е����������Ϣ������ͬ���������͡�
        * - **����**��
        *   - `_Task_rref`: Ҫִ�е��������Ϣ������Ϊ _TyItem����
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
        * - **����**��������Ϣѭ������������е��������Ϣ��ֱ�����յ��˳���Ϣ��
        * - **����**���ޡ�
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
                    // ����������˳���־�������������Ϊ�����˳�����
                    break;
                }

                // ����һ����Ϣ
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

    **���**��
    `_Timer_message_loop` �̳��� `_Message_loop`����չ�˶�ʱ�����ȹ��ܣ�֧��һ���ԡ��̶�Ƶ�ʺ͹̶��ӳٵĶ�ʱ�����������û����ö�ʱ���Դ����ص�����Ϣ��������������������ӳ�ִ�г�������ͨ�� `_Duration_t` �� `_Clock_t` ģ������ṩ����ʱ�侫�Ⱥ�ʱ��ѡ��

    **��Ҫ����**��
    - ֧�ֶ��ֶ�ʱ��ģʽ��`call_once`��`fixed_rate`��`fixed_delay`����
    - �ṩ `set_timer` �� `kill_timer` ��������ʱ���������ڡ�
    - �����麯�� `_On_timer` �����Զ��嶨ʱ���¼�����
    - ʹ�����ȼ�����ȷ����ʱ����ʱ��˳�򴥷���

    - **_Arg_t**
    - **��;**��ָ����ʱ���¼�������Ĳ������ͣ����ݸ� `_On_timer` ������ʱ���ص���
    - **Ĭ��ֵ**��`std::any`������洢�������Ͳ�����֧�ֶ�̬���ʹ���
    - **ʹ�ó���**��
        - �̳��� `_Message_loop`����;�� `_Message_loop::_Arg_t` ��ͬ��
        - �����ڶ�ʱ���������������ݣ�����������á����ö��󣩡�
        - Ӱ�� `set_timer` �� `_On_timer` �Ĳ������ݡ�

    - **_Result_t**
    - **��;**��ָ����Ϣ����ķ���ֵ���ͣ��̳��� `_Message_loop`����ʱ���¼�ͨ����ֱ��ʹ�á�
    - **Ĭ��ֵ**��`std::any`���������������ͽ����
    - **ʹ�ó���**��
        - �̳��� `_Message_loop`����Ҫ������Ϣ������Ƕ�ʱ����
        - ��ָ������������������Ϣ```

    System: **_Result_t**
    - **��;**��ָ����Ϣ����ķ���ֵ���ͣ��̳��� `_Message_loop`, ��ʱ���¼�ͨ����ֱ��ʹ�á�
    - **Ĭ��ֵ**��`std::any`, �������������ͽ����
    - **ʹ�ó���**��
        - �̳��� `_Message_loop`, ��Ҫ������Ϣ������Ƕ�ʱ����
        - ��ָ���������ͣ��� `int`�������Ʒ���ֵ���Ͳ�������Ͱ�ȫ�ԡ�
        - Ӱ�� `send`, `post_await`, �� `_On_message` �ķ���ֵ����

    - **_Key_t**
    - **��;**��ָ����ʱ������Ϣ�ļ����ͣ����ڱ�ʶ��ʱ���¼�����Ϣ�����ݸ� `_On_timer` �� `_On_message` ������
    - **Ĭ��ֵ**��`uintptr_t`, �ṩ�������͵ı�ʶ���ʺ�ö��ֵ������
    - **ʹ�ó���**��
        - �̳��� `_Message_loop`, ��;�� `_Message_loop::_Key_t` ��ͬ��
        - ���ڱ�ʶ��ʱ������ `CM_TIMER1`������Ϣ��
        - Ӱ�� `set_timer`, `kill_timer`, �� `_On_timer` �ļ�����

    - **_Duration_t**
    - **��;**��ָ����ʱ����ʱ�������ͣ����嶨ʱ���Ĵ������Ⱥ�ʱ�䵥λ��
    - **Ĭ��ֵ**��`std::chrono::nanoseconds`, �ṩ���뼶���ȣ��ʺϸ߾��ȶ�ʱ����
    - **ʹ�ó���**��
        - ���ƶ�ʱ����ʱ����������롢�룩��
        - ��ָ���������ͣ��� `std::chrono::milliseconds`���Ե������Ȼ�򻯴��롣
        - Ӱ�� `set_timer`, `set_timer_once`, `set_timer_fixed_delay`, �� `set_timer_fixed_rate` ��ʱ�������

    - **_Clock_t**
    - **��;**��ָ����ʱ��ʹ�õ�ʱ�����ͣ�����ʱ������ʱ��Դ��
    - **Ĭ��ֵ**��`std::chrono::steady_clock`, �ṩ����������ʱ�䣬�ʺ϶�ʱ�����ȡ�
    - **ʹ�ó���**��
        - ʹ�� `std::chrono::steady_clock` ȷ����ʱ������ϵͳʱ�����Ӱ�졣
        - ��ָ������ʱ�ӣ��� `std::chrono::system_clock`����֧�ֻ���ϵͳʱ��Ķ�ʱ����
        - Ӱ�춨ʱ���Ĵ���ʱ�����͵��Ⱦ��ȡ�
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
            call_once,    // һ���Ե���
            fixed_rate,   // �ƻ���������һ�μƻ�ʱ�� + ���ڣ����ܲ�����
            fixed_delay,  // �̶��ӳ٣���һ�λص����� + ���ڣ���������
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
        * - **����**���麯��������ʱ���¼������û���д��ʵ���Զ��嶨ʱ���߼���
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݵĶ�ʱ�����ݡ�
        */
        virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept {}
    public:

        /*
        * ### set_timer����������
        * - **����**�����ö�ʱ����ָ������ʱ�䡢�����͵���ģʽ������ʱ���� _On_timer��
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥��ʱ�䡣
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݸ� _On_timer��Ĭ��Ϊ�ա�
        *   - `mode`: ����ģʽ��call_once��fixed_rate��fixed_delay����Ĭ��Ϊ fixed_delay��
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
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
        * - **����**������һ���Զ�ʱ��������һ�κ��Զ��Ƴ������� _On_timer��
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥��ʱ�䡣
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݸ� _On_timer��Ĭ��Ϊ�ա�
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
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
        * - **����**�����ù̶��ӳٶ�ʱ����ÿ�δ��������¼����������� _On_timer��
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥�������
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݸ� _On_timer��Ĭ��Ϊ�ա�
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
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
        * - **����**�����ù̶�Ƶ�ʶ�ʱ�������̶�ʱ�������������� _On_timer��
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥��Ƶ�ʡ�
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݸ� _On_timer��Ĭ��Ϊ�ա�
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
        */
        template < typename __Rep, typename __Period >
        inline bool set_timer_fixed_rate(
            _TyKey key,
            const std::chrono::duration<__Rep, __Period> elapse,
            _TyArg&& arg = {}) {
            return set_timer(key, elapse, std::move(arg), schedule_mode::fixed_rate);
        }

        /*
        * ### set_timer�����ص���
        * - **����**�����ö�ʱ����ָ������ʱ�䡢�ص������Ͳ���������ʱִ�лص���
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥��ʱ�䡣
        *   - `mode`: ����ģʽ��call_once��fixed_rate��fixed_delay����
        *   - `func`: �ɵ��ö��󣬶��嶨ʱ������ʱ���߼���
        *   - `args`: �ɱ���������ݸ� func �Ĳ���������� _Arg_t��
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
        * - **��ע**�������ʱ���Ѵ��ڣ��������������Ϣ��
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
        * - **����**������һ���Զ�ʱ��������һ�κ��Զ��Ƴ������� _On_timer��
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥��ʱ�䡣
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݸ� _On_timer��Ĭ��Ϊ�ա�
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
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
        * - **����**�����ù̶��ӳٶ�ʱ����ÿ�δ��������¼����������� _On_timer��
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥�������
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݸ� _On_timer��Ĭ��Ϊ�ա�
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
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
        * - **����**�����ù̶�Ƶ�ʶ�ʱ�������̶�ʱ�������������� _On_timer��
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t�������ֶ�ʱ����
        *   - `elapse`: ʱ����������Ϊ std::chrono::duration�������崥��Ƶ�ʡ�
        *   - `arg`: ��ʱ������������Ϊ _TyArg�������ݸ� _On_timer��Ĭ��Ϊ�ա�
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ����á�
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
        * - **����**���Ƴ�ָ����ʱ����ֹͣ�䴥����
        * - **����**��
        *   - `key`: ��ʱ����ʶ������Ϊ _Key_t����ָ��Ҫ�Ƴ��Ķ�ʱ����
        * - **����ֵ**��`bool`����ʾ��ʱ���Ƿ�ɹ��Ƴ���
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
        * - **����**����д _Message_loop �� dispatch��������Ϣѭ��������ʱ���¼���
        * - **����**���ޡ�
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
                    // ����������˳���־�������������Ϊ�����˳�����
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
        * - **����**��ִ�ж�ʱ�����񣬴���ʱ�������ص���
        * - **����**��
        *   - `_Timer_ref`: ��ʱ����������Ϊ _TyTimer����������ʱ����Ϣ��
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
        * - **����**��������Ķ�ʱ��������ȡ�����紥���Ķ�ʱ����
        * - **����**���ޡ�
        * - **����ֵ**��`_TyTimerPtr`��ָ�����紥���Ķ�ʱ����������Ϊ���򷵻ؿ�ָ�롣
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
        * - **����**������ʱ����������Ķ�ʱ�����У�������ʱ������
        * - **����**��
        *   - `_Timer_ptr`: ��ʱ��ָ�루����Ϊ _TyTimerPtr����ָ��Ҫ����Ķ�ʱ����
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
        * - **����**�����ָ����ʱ���Ƿ��ѱ��Ƴ���
        * - **����**��
        *   - `_Timer_ptr`: ��ʱ��ָ�루����Ϊ _TyTimerPtr����ָ��Ҫ���Ķ�ʱ����
        * - **����ֵ**��`bool`������ʱ���ѱ��Ƴ����� true�����򷵻� false��
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

    **���**��
    `_Thread_loop` ��һ�����߳���Ϣѭ���࣬�� `_Base_t`��Ĭ�� `_Message_loop`������Ϣѭ�������ڶ����ĺ�̨�߳��С����������̲߳�����ȷ������˳��ִ�У��ʺ���Ҫ�̸߳���ĳ������� GUI �¼������򵥵ĺ�̨������ȡ�

    **��Ҫ����**��
    - ���߳�������Ϣѭ������֤������ִ�С�
    - �ṩ `start`��`stop`��`join` ���������߳��������ڡ�
    - ֧���Զ��� `_Base_t`���� `_Timer_message_loop`������չ���ܡ�
    - �̰߳�ȫ����Ͷ�ݣ��򻯿��߳�ͨ�š�

    - **_Base_t**
    - **��;**��ָ��������Ϣѭ�����ͣ����� `_Thread_loop` ʹ�õ���Ϣѭ��ʵ�֡�
    - **Ĭ��ֵ**��`message_loop<>`, ʹ��Ĭ�ϵ� `_Message_loop` ���ã�`std::any` �����ͷ���ֵ��`uintptr_t` ������
    - **ʹ�ó���**��
        - Ĭ��ʹ�� `_Message_loop` �ṩ������Ϣѭ�����ܡ�
        - ��ָ�� `_Timer_message_loop` ���Զ�����Ϣѭ��������֧�ֶ�ʱ����������չ���ܡ�
        - Ӱ�� `_Thread_loop` ����Ϣ���������ַ�������
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
        * - **����**��������̨�߳�������Ϣѭ���������������Ϣ��
        * - **����**���ޡ�
        * - **����ֵ**��`bool`����ʾ�߳��Ƿ�ɹ�������
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
        * - **����**��Ͷ���˳���Ϣ��ֹͣ��Ϣѭ�����ȴ��߳̽�����������Դ��
        * - **����**��
        *   - `_Cancel_pending`: ����ֵ����Ϊ true����ն�����δ���������Ĭ��Ϊ false��
        */
        void stop(bool _Cancel_pending = false) {
            if (joinable()) {
                _Base_t::post_quit_message(_Cancel_pending);
                join();
            }
        }

        /*
        * ### joinable
        * - **����**������߳��Ƿ�ɼ��룬�ж��߳�����״̬��
        * - **����**���ޡ�
        * - **����ֵ**��`bool`�����߳̿ɼ��루�����л��ѽ�����δ�����򷵻� true��
        */
        inline bool joinable() const noexcept {
            return _My_threads.joinable();
        }

        /*
        * ### join
        * - **����**���ȴ��߳̽�����ȷ���̰߳�ȫ�˳���
        * - **����**���ޡ�
        */
        inline void join() {
            return _My_threads.join();
        }
    protected:
        std::thread _My_threads;
    };
    /*
    ## 4. _Thread_pool_loop

    **���**��
    `_Thread_pool_loop` ��һ���̳߳���Ϣѭ���࣬���ö���̲߳������� `_Base_t`��Ĭ�� `_Message_loop`������Ϣ��������֧�ֶ�̬�����߳��������Ż��߸��س��������ܣ������ڷ�����Ӧ�û򲢷�����������

    **��Ҫ����**��
    - ���̲߳���ִ������������������
    - �ṩ `start`��`stop`��`get_thread_count` ���������̳߳ء�
    - ֧���Զ��� `_Base_t`���� `_Timer_message_loop`������չ���ܡ�
    - �Զ�����Ӳ�������ԣ��Ż���Դ�����ʡ�

    - **_Base_t**
    - **��;**��ָ��������Ϣѭ�����ͣ����� `_Thread_pool_loop` ʹ�õ���Ϣѭ��ʵ�֡�
    - **Ĭ��ֵ**��`message_loop<>`, ʹ��Ĭ�ϵ� `_Message_loop` ���ã�`std::any` �����ͷ���ֵ��`uintptr_t` ������
    - **ʹ�ó���**��
        - Ĭ��ʹ�� `_Message_loop` �ṩ������Ϣѭ�����ܡ�
        - ��ָ�� `_Timer_message_loop` ���Զ�����Ϣѭ��������֧�ֶ�ʱ����������չ���ܡ�
        - Ӱ�� `_Thread_pool_loop` ����Ϣ����Ͳ�������ַ�������
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
        * - **����**�������̳߳أ�����ָ���������߳�������Ϣѭ����
        * - **����**��
        *   - `_Thread_count`: �߳�������Ĭ��ΪӲ�������߳�����
        * - **����ֵ**��`bool`����ʾ�̳߳��Ƿ�ɹ�������
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
         * - **����**����ȡ�̳߳��е��߳�������
         * - **����**���ޡ�
         * - **����ֵ**��`std::size_t`����ǰ�̳߳ص��߳�����
         */
        std::size_t get_thread_count() const {
            return _My_threads.size();
        }

        /*
        * ### stop
        * - **����**��Ͷ���˳���Ϣ��ֹͣ�̳߳ز��ȴ������߳̽�����������Դ��
        * - **����**��
        *   - `_Cancel_pending`: ����ֵ����Ϊ true����ն�����δ���������Ĭ��Ϊ false��
        */
        void stop(bool _Cancel_pending = false) {
            if (joinable()) {
                _Base_t::post_quit_message(_Cancel_pending);
                join();
            }
        }

        /*
        * ### joinable
        * - **����**������Ƿ���ڿɼ�����̣߳��ж��̳߳�����״̬��
        * - **����**���ޡ�
        * - **����ֵ**��`bool`�������ڿɼ����߳��򷵻� true��
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
        * - **����**���ȴ������߳̽�����ȷ���̳߳ذ�ȫ�˳���
        * - **����**���ޡ�
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
    // ������Ϣѭ��
    template <
        typename Arg = std::any,
        typename Result = std::any,
        typename Key = uintptr_t
    >
    using message_loop = _Message_loop<Arg, Result, Key>;

    // ��ʱ��֧�ֵ���Ϣѭ��
    template <
        typename Arg = std::any,
        typename Result = std::any,
        typename Key = uintptr_t,
        typename Duration = std::chrono::nanoseconds,
        typename Clock = std::chrono::steady_clock
    >
    using timer_message_loop = _Timer_message_loop<Arg, Result, Key, Duration, Clock>;

    // �̳߳ذ汾��Ϣѭ��
    template <
        typename Base = message_loop<>
    >
    using thread_pool_loop = _Thread_pool_loop<Base>;

    // ���̺߳�̨�̰߳汾��Ϣѭ��
    template <
        typename Base = message_loop<>
    >
    using thread_loop = _Thread_loop<Base>;

}

// ================================================================================
// ===== ��Ϣӳ�亯���ӿں� =====

#define XDECLARE_ON_MESSAGE() \
    virtual _TyResult _On_message(_TyKey key, _TyArg& arg) noexcept override

#define XDEFINE_ON_MESSAGE(_Class) \
    _Class::_TyResult _Class::_On_message(_TyKey key, _TyArg& arg) noexcept

// ===== ��Ϣӳ��ʵ�ֽṹ�� =====

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

// ===== ��ʱ��ӳ�亯���ӿں� =====

#define XDECLARE_ON_TIMER() \
    virtual void _On_timer(_TyKey key, _TyArg& arg) noexcept override

#define XDEFINE_ON_TIMER(_Class) \
    void _Class::_On_timer(_TyKey key, _TyArg& arg) noexcept

// ===== ��ʱ��ӳ��ʵ�ֽṹ�� =====

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