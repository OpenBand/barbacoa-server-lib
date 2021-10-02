#include <server_lib/application.h>
#include <server_lib/logging_helper.h>
#include <server_lib/asserts.h>
#include <server_lib/emergency_helper.h>
#include <server_lib/platform_config.h>
#include <server_lib/thread_sync_helpers.h>

#include "logging_trace.h"

#include <boost/filesystem.hpp>
#include <mutex>
#include <condition_variable>
#include <thread>

#include <cstring>

#include <server_clib/app.h>
#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <pthread.h>
#include <sys/syscall.h>
#include <iostream>
#endif

#include <signal.h>

#include "logger_set_internal_group.h"

namespace server_lib {

namespace impl {

    struct signal_data
    {
        using control_signal = application::control_signal;

        union {
            int signo = 0;
            control_signal signal;
            char path_data[PATH_MAX];
        } data;

        enum class data_type
        {
            null = 0,
            fail,
            exit,
            usersignal
        };
        data_type type = data_type::null;

        static signal_data make_empty_data();
        static signal_data make_fail_data(const char*);
        static signal_data make_exit_data(const int signo);
        static signal_data make_control_data(const control_signal);
    };

    signal_data signal_data::make_empty_data()
    {
        signal_data r;
        memset(r.data.path_data, 0, PATH_MAX);
        return r;
    }

    signal_data signal_data::make_fail_data(const char* crash_dump_file_path)
    {
        signal_data r;
        r.type = data_type::fail;
        memset(r.data.path_data, 0, PATH_MAX);
        if (crash_dump_file_path)
        {
            std::strncpy(r.data.path_data, crash_dump_file_path, PATH_MAX - 1);
        }
        return r;
    }

    signal_data signal_data::make_exit_data(const int signo)
    {
        signal_data r;
        r.type = data_type::exit;
        memset(r.data.path_data, 0, PATH_MAX);
        r.data.signo = signo;
        return r;
    }

    signal_data signal_data::make_control_data(const control_signal sig)
    {
        signal_data r;
        r.type = data_type::usersignal;
        memset(r.data.path_data, 0, PATH_MAX);
        r.data.signal = sig;
        return r;
    }

    static signal_data last_signal_data = signal_data::make_empty_data();
    static char crash_dump_file_path[PATH_MAX] = { 0 };

    static std::function<void(int)> sig_callback = nullptr;
    static std::function<void(void)> atexit_callback = nullptr;
    static std::function<void(void)> fail_callback = nullptr;

    void sig_callback_wrapper(int signo)
    {
        if (sig_callback)
            sig_callback(signo);
    }

    void atexit_callback_wrapper()
    {
        if (atexit_callback)
            atexit_callback();
    }

    void fail_callback_wrapper()
    {
        if (fail_callback)
            fail_callback();
    }

} // namespace impl

class __application_impl
{
    static void application_abort();

    using control_signal = application::control_signal;

    void init_crash_dump_generation(const char* path_to_dump_file)
    {
        std::strncpy(impl::crash_dump_file_path, path_to_dump_file, PATH_MAX - 1);
    }
    static const char* get_crash_dump_file_name();
    static void set_signal_data(impl::signal_data&&);
    static impl::signal_data get_signal_data();

    static void process_signal(int signal);
    static void process_atexit();
    static void process_fail();

public:
    __application_impl()
    {
        SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");
        cleanup();
    }

    void init(const char* path_to_dump_file, bool daemon, bool lock_io)
    {
        SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");

        std::string executable_name = emergency_helper::get_executable_name();
        if (executable_name.empty())
            executable_name = "MAIN";

        _main_el.change_thread_name(executable_name);

        if (path_to_dump_file)
            init_crash_dump_generation(path_to_dump_file);

        impl::sig_callback = [this](int signo) { __application_impl::process_signal(signo); };
        impl::atexit_callback = [this]() { __application_impl::process_atexit(); };
        impl::fail_callback = [this]() { __application_impl::process_fail(); };

        srv_c_app_init_default_signals_should_register();

        if (daemon)
        {
            srv_c_app_mt_init_daemon(impl::atexit_callback_wrapper,
                                     impl::sig_callback_wrapper,
                                     impl::fail_callback_wrapper);
        }
        else
        {
            srv_c_app_mt_init(impl::atexit_callback_wrapper,
                              impl::sig_callback_wrapper,
                              impl::fail_callback_wrapper,
                              lock_io ? TRUE : FALSE);
        }
    }

    using start_callback_type = application::start_callback_type;
    using exit_callback_type = application::exit_callback_type;
    using fail_callback_type = application::fail_callback_type;
    using control_callback_type = application::control_callback_type;

    int run();

    bool is_running() const
    {
        return _main_el.is_running();
    }

    static __application_impl& instance();

    void on_start(start_callback_type&& callback)
    {
        SRV_ASSERT(!_main_el.is_running(), "Call before running");

        _start_callback = callback;
    }

    void on_exit(exit_callback_type&& callback)
    {
        SRV_ASSERT(!_main_el.is_running(), "Call before running");

        _exit_callback = callback;
    }

    void on_fail(fail_callback_type&& callback)
    {
        SRV_ASSERT(!_main_el.is_running(), "Call before running");

        _fail_callback = callback;
    }

    void on_control(control_callback_type&& callback)
    {
        SRV_ASSERT(!_main_el.is_running(), "Call before running");

        _control_callback = callback;
    }

    void wait()
    {
        if (!_main_el.is_running())
        {
            std::unique_lock<std::mutex> lck(_wait_started_condition_lock);
            _wait_started_condition.wait(lck, [& el = _main_el] { return el.is_running(); });
        }
    }

    main_loop& loop()
    {
        return _main_el;
    }

    void stop()
    {
        _main_el.stop(); //makes 'run' exit
    }

private:
    void run_impl();

    void process_fail(const char* dump_file_path)
    {
        if (_fail_callback)
        {
            SRV_LOGC_TRACE(__FUNCTION__ << "(" << dump_file_path << ")");

            auto fail_callback = _fail_callback;
            fail_callback(dump_file_path);
        }
    }

    void process_exit()
    {
        if (_main_el.is_running())
        {
            SRV_LOGC_TRACE(__FUNCTION__);

            if (_exit_callback)
            {
                auto exit_callback = _exit_callback;
                _main_el.wait_async_no_result([exit_callback]() {
                    exit_callback();
                });
            }

            stop();
        }
    }

    void process_control(control_signal signal)
    {
        if (_control_callback)
        {
            SRV_LOGC_TRACE(__FUNCTION__);

            auto control_callback = _control_callback;
            _main_el.post([control_callback, signal]() { control_callback(signal); });
        }
        else
        {
            SRV_LOGC_TRACE(__FUNCTION__ << " - Ignored");
        }
    }

    void cleanup()
    {
        _signal_thread_initiated = false;
        _signal_thread_terminating = false;
    }

private:
    main_loop _main_el;
    std::thread _signal_thread;
    std::atomic_bool _signal_thread_initiated;
    std::atomic_bool _signal_thread_terminating; //if it is start terminating at least

    std::condition_variable _wait_started_condition;
    std::mutex _wait_started_condition_lock;

    start_callback_type _start_callback = nullptr;
    exit_callback_type _exit_callback = nullptr;
    fail_callback_type _fail_callback = nullptr;
    control_callback_type _control_callback = nullptr;
}; // namespace server_lib

void __application_impl::application_abort()
{
    SRV_TRACE_SIGNAL(__FUNCTION__);

#if defined(SERVER_LIB_PLATFORM_LINUX)
    ::srv_c_app_abort();
#else
    abort();
#endif
}

const char* __application_impl::get_crash_dump_file_name()
{
    if (impl::crash_dump_file_path[0])
        return impl::crash_dump_file_path;
    else
        return nullptr;
}

void __application_impl::set_signal_data(impl::signal_data&& signal_data)
{
    impl::last_signal_data = std::move(signal_data);
}

impl::signal_data __application_impl::get_signal_data()
{
    return impl::last_signal_data;
}

void __application_impl::process_signal(int signal)
{
    if (srv_c_is_fail_signal(signal))
    {
        SRV_TRACE_SIGNAL("Got fail signal");

        bool raw_dump_saved = false;
        if (get_crash_dump_file_name())
        {
            raw_dump_saved = emergency_helper::save_raw_dump_s(get_crash_dump_file_name()) > 0;
        }

        if (raw_dump_saved && get_crash_dump_file_name())
        {
            set_signal_data(impl::signal_data::make_fail_data(get_crash_dump_file_name()));
        }
        else
        {
            set_signal_data(impl::signal_data::make_fail_data(nullptr));
        }

#if !defined(SERVER_LIB_PLATFORM_LINUX)
        __application_impl::application_abort();
#endif
    }
    else
    {
#if defined(SERVER_LIB_PLATFORM_LINUX)
        using control_signal = application::control_signal;

        switch (signal)
        {
        case SIGUSR1:

            SRV_TRACE_SIGNAL("Got USR1 control signal");

            set_signal_data(impl::signal_data::make_control_data(control_signal::USR1));
            break;
        case SIGUSR2:

            SRV_TRACE_SIGNAL("Got USR2 control signal");

            set_signal_data(impl::signal_data::make_control_data(control_signal::USR2));
            break;
        default:
#else
        {
#endif
            SRV_TRACE_SIGNAL("Got exit signal");

            set_signal_data(impl::signal_data::make_exit_data(signal));
        }
    }
}

void __application_impl::process_atexit()
{
    SRV_TRACE_SIGNAL(__FUNCTION__);

    if (logger::check_instance())
        logger::instance().lock(); //boost logger failed after 'exit' instructions

    auto& app = instance();
    if (app.is_running())
    {
        SRV_ASSERT(false, "std::exit is not allowed to stop application. Use application.stop instead");
    }
}

void __application_impl::process_fail()
{
    SRV_TRACE(__FUNCTION__);

    auto& app = instance();
    if (app.is_running())
    {
        using Callbackdata_type
            = impl::signal_data::data_type;

        auto last_signal_data = get_signal_data();
        if (last_signal_data.type == Callbackdata_type::fail)
        {
            app.process_fail(last_signal_data.data.path_data);
        }
    }
}

int __application_impl::run()
{
    if (_main_el.is_running())
        return exit_code_error;

    run_impl();
    return exit_code_ok;
}

void __application_impl::run_impl()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    pthread_t signal_thread_id = 0;
#endif

    std::thread signal_thread([&]() {
        SRV_LOGC_TRACE("Signal thread has just started");

#if defined(SERVER_LIB_PLATFORM_LINUX)
        signal_thread_id = pthread_self();
#endif

        using Callbackdata_type
            = impl::signal_data::data_type;

        auto process_handler = [this]() -> bool {
            auto last_signal_data = get_signal_data();
            switch (last_signal_data.type)
            {
            case Callbackdata_type::exit:
                this->_signal_thread_terminating = true;
                this->process_exit();
                break;
            case Callbackdata_type::usersignal:
                this->process_control(last_signal_data.data.signal);
                break;
            default:;
            }
            return this->_signal_thread_terminating.load();
        };
        bool terminated = false;

#if defined(SERVER_LIB_PLATFORM_LINUX)
        pthread_setname_np(pthread_self(), "signal");
#endif

        if (!terminated)
        {
            bool expected = false;
            while (!this->_signal_thread_initiated.compare_exchange_weak(expected, true))
                spin_loop_pause();

            SRV_LOGC_TRACE("Signal thread has completely initiated");
        }

        while (!terminated)
        {
            // Only here async signals can arise
            srv_c_app_mt_wait_sig_callback(impl::sig_callback_wrapper);

            SRV_LOGC_INFO("Got signal in signal thread");

            terminated = process_handler();
        }

        SRV_LOGC_TRACE("Signal thread has stopped");
    });
    _signal_thread.swap(signal_thread);

    bool expected = true;
    while (!this->_signal_thread_initiated.compare_exchange_weak(expected, true))
        spin_loop_pause();
    // Here signal thread has started, signal_thread_id has intialized

    // Stuck into main loop
    _main_el.start([this]() {
        SRV_LOGC_TRACE("Application has started");
        if (_start_callback)
            _start_callback();
        _wait_started_condition.notify_all();
    });

    // Here smth. has stopped main loop
    SRV_LOGC_TRACE("Application is stopping");

    if (_signal_thread_terminating.load())
        _signal_thread.join();
    else
    {
        // Main loop has stopped not by signal thread and signal thread
        // must be informed
        SRV_LOGC_TRACE("Throw notification to signal thread");

#if defined(SERVER_LIB_PLATFORM_LINUX)
        // It should send signal to exact thread because the signals was blocked
        // for others
        SRV_ASSERT(0 == pthread_kill(signal_thread_id, SIGTERM));
#else
        SRV_ASSERT(0 == raise(SIGTERM));
#endif
        _signal_thread.join();

        // It was not called from signal thread because main loop has not run
        if (_exit_callback)
            _exit_callback();
    }

    SRV_LOGC_INFO("Application has stopped");
    cleanup();
}

class __internal_application_manager
{
public:
    static __application_impl singleton_app_impl;
    static application singleton_app;
};

__application_impl __internal_application_manager::singleton_app_impl;

__application_impl& __application_impl::instance()
{
    return __internal_application_manager::singleton_app_impl;
}

application::application()
{
    SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");
}

application& application::init(const char* path_to_dump_file, bool daemon, bool lock_io)
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    auto this_pid = getpid();
    namespace fs = boost::filesystem;
    bool forbidden_threads = false;
    for (fs::directory_entry& th : fs::directory_iterator("/proc/self/task"))
    {
        auto p = th.path();
        if (this_pid != atol(p.filename().generic_string().c_str()))
        {
            forbidden_threads = true;
            std::cerr << "Not permitted because detected forbidden thread: " << p.filename().generic_string() << std::endl;
            break;
        }
    }
    SRV_ASSERT(!forbidden_threads,
               "Application must be initiated in main thread "
               "before creation of any other thread "
               "to make signal handling safe in multithreaded environment");
#endif

    auto& app_impl = __internal_application_manager::singleton_app_impl;
    auto& app = __internal_application_manager::singleton_app;
    app_impl.init(path_to_dump_file, daemon, lock_io);
    return app;
}

int application::run()
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    return app_impl.run();
}

bool application::is_running()
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    return app_impl.is_running();
}

application& application::on_start(start_callback_type&& callback)
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    app_impl.on_start(std::forward<start_callback_type>(callback));
    return *this;
}

application& application::on_exit(exit_callback_type&& callback)
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    app_impl.on_exit(std::forward<exit_callback_type>(callback));
    return *this;
}

application& application::on_fail(fail_callback_type&& callback)
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    app_impl.on_fail(std::forward<fail_callback_type>(callback));
    return *this;
}

application& application::on_control(control_callback_type&& callback)
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    app_impl.on_control(std::forward<control_callback_type>(callback));
    return *this;
}

void application::wait()
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    app_impl.wait();
}

main_loop& application::loop()
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    return app_impl.loop();
}

void application::stop()
{
    auto& app_impl = __internal_application_manager::singleton_app_impl;
    app_impl.stop();
}

application __internal_application_manager::singleton_app;

} // namespace server_lib
