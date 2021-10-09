#include "application_impl.h"

#include <server_lib/asserts.h>
#include <server_lib/emergency_helper.h>
#include <server_lib/platform_config.h>
#include <server_lib/thread_sync_helpers.h>

#include <boost/filesystem.hpp>

#include <cstring>

#include <server_clib/app.h>
#include <ssl_helpers/utils.h>
#include <ssl_helpers/encoding.h>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <pthread.h>
#include <sys/syscall.h>
#include <iostream>

#include <unistd.h>
#include <sys/resource.h>

#endif

#include <signal.h>

#include "logger_set_internal_group.h"

#ifndef NDEBUG
#define TRACE_SIG_NUMBER(signo)                      \
    int32_t s__                                      \
        = 0x3030 | (signo / 10) | (signo % 10) << 8; \
    SRV_TRACE_SIGNAL_((char*)&s__);                  \
    SRV_TRACE_SIGNAL_("\n")
#else
#define TRACE_SIG_NUMBER(signo)
#endif

#define EXIT_FAILURE_SIG_BASE_ 128

namespace server_lib {
namespace {

    struct __signal_data
    {
        using control_signal = application::control_signal;

        union {
            int signo = 0;
            control_signal signal;
        } data;

        enum class data_type
        {
            null = 0,
            fail,
            exit,
            usersignal
        };
        data_type type = data_type::null;

        char path_data[PATH_MAX] = { 0 };

        static __signal_data make_empty_data();
        static __signal_data make_fail_data(const int signo, const char* = nullptr);
        static __signal_data make_exit_data(const int signo);
        static __signal_data make_control_data(const control_signal);
    };

    __signal_data __signal_data::make_empty_data()
    {
        return {};
    }

    __signal_data __signal_data::make_fail_data(const int signo, const char* stdump_file_path)
    {
        __signal_data r;
        r.type = data_type::fail;
        r.data.signo = signo;
        memset(r.path_data, 0, PATH_MAX);
        if (stdump_file_path)
        {
            std::strncpy(r.path_data, stdump_file_path, PATH_MAX - 1);
        }
        return r;
    }

    __signal_data __signal_data::make_exit_data(const int signo)
    {
        __signal_data r;
        r.type = data_type::exit;
        r.data.signo = signo;
        return r;
    }

    __signal_data __signal_data::make_control_data(const control_signal sig)
    {
        __signal_data r;
        r.type = data_type::usersignal;
        r.data.signal = sig;
        return r;
    }

    static __signal_data last_signal_data = __signal_data::make_empty_data();
    static char stdump_file_path[PATH_MAX] = { 0 };

    static std::function<void(int)> sig_callback = nullptr;
    static std::function<void(void)> atexit_callback = nullptr;
    static std::function<void(void)> fail_callback = nullptr;

    void __sig_callback_wrapper(int signo)
    {
        if (sig_callback)
            sig_callback(signo);
    }

    void __atexit_callback_wrapper()
    {
        if (atexit_callback)
            atexit_callback();
    }

    void __fail_callback_wrapper()
    {
        if (fail_callback)
            fail_callback();
    }

    void __set_signal_data(__signal_data&& __signal_data)
    {
        last_signal_data = std::move(__signal_data);
    }

    __signal_data __get_signal_data()
    {
        return last_signal_data;
    }

    void __init_stdump_generation(const std::string& path_to_dump_file)
    {
        std::strncpy(stdump_file_path, path_to_dump_file.c_str(), PATH_MAX - 1);
    }

    const char* __get_stdump_file_name()
    {
        if (stdump_file_path[0])
            return stdump_file_path;
        else
            return nullptr;
    }

    bool __enable_corefile(bool disable_excl_policy)
    {
        // Core dump creation is OS responsibility and OS specific
#if defined(SERVER_LIB_PLATFORM_LINUX)
        try
        {
            struct rlimit old_r, new_r;

            new_r.rlim_cur = RLIM_INFINITY;
            new_r.rlim_max = RLIM_INFINITY;

            SRV_ASSERT(prlimit(getpid(), RLIMIT_CORE, &new_r, &old_r) != -1);

            if (disable_excl_policy)
            {
                namespace fs = boost::filesystem;

                auto prev_core = fs::current_path() / "core";
                if (fs::exists(prev_core))
                {
                    std::ifstream core(prev_core.generic_string(), std::ifstream::binary);

                    char elf_header[4];
                    SRV_ASSERT(core && core.read(elf_header, sizeof(elf_header)), "File is not readable");

                    auto elf_hex = ssl_helpers::to_hex(std::string(elf_header, sizeof(elf_header)));
                    SRV_ASSERT(elf_hex == "7f454c46", "Wrong file type");

                    core.close();

                    auto tm = ssl_helpers::to_iso_string(fs::last_write_time(prev_core));
                    auto renamed = prev_core;
                    renamed = renamed.parent_path() / (std::string("core.") + tm);
                    fs::rename(prev_core, renamed);
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            return false;
        }
#endif
        return true;
    }

} // namespace

class __internal_application_manager
{
public:
    static application_impl singleton_app_impl;
    static application singleton_app;
};

application_impl __internal_application_manager::singleton_app_impl;
application __internal_application_manager::singleton_app;

application_impl::application_impl()
{
    SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");
    _exit_code = 0;
    cleanup();
}

application_impl& application_impl::instance()
{
    return __internal_application_manager::singleton_app_impl;
}

application& application_impl::app_instance()
{
    return __internal_application_manager::singleton_app;
}

void application_impl::set_config(const application_config& config)
{
    _config = config;
}

void application_impl::init()
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

    SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");

    std::string executable_name = emergency::get_executable_name();
    if (executable_name.empty())
        executable_name = "MAIN";

    _main_el.change_thread_name(executable_name);

    if (config()._enable_corefile)
    {
        SRV_ASSERT(__enable_corefile(config()._corefile_disable_excl_policy), "Incompatible with current corefile creation system");
    }

    if (!config()._path_to_stdump_file.empty())
        __init_stdump_generation(config()._path_to_stdump_file);

    sig_callback = [this](int signo) { application_impl::process_signal(signo); };
    atexit_callback = [this]() { application_impl::process_atexit(); };
    fail_callback = [this]() { application_impl::process_fail(); };

    srv_c_app_init_default_signals_should_register();

    if (config()._daemon)
    {
        srv_c_app_mt_init_daemon(__atexit_callback_wrapper,
                                 __sig_callback_wrapper,
                                 __fail_callback_wrapper,
                                 config()._corefile_fail_thread_only ? TRUE : FALSE);
    }
    else
    {
        srv_c_app_mt_init(__atexit_callback_wrapper,
                          __sig_callback_wrapper,
                          __fail_callback_wrapper,
                          config()._corefile_fail_thread_only ? TRUE : FALSE,
                          config()._lock_io ? TRUE : FALSE);
    }
}

int application_impl::run()
{
    if (_main_el.is_running())
        return exit_code_error;

    run_impl();
    return exit_code_ok;
}

void application_impl::on_start(start_callback_type&& callback)
{
    SRV_ASSERT(!_main_el.is_running(), "Call before running");

    _start_callback = callback;
}

void application_impl::on_exit(exit_callback_type&& callback)
{
    SRV_ASSERT(!_main_el.is_running(), "Call before running");

    _exit_callback = callback;
}

void application_impl::on_fail(fail_callback_type&& callback)
{
    SRV_ASSERT(!_main_el.is_running(), "Call before running");

    _fail_callback = callback;
}

void application_impl::on_control(control_callback_type&& callback)
{
    SRV_ASSERT(!_main_el.is_running(), "Call before running");

    _control_callback = callback;
}

void application_impl::wait()
{
    if (!_main_el.is_run())
    {
        std::unique_lock<std::mutex> lck(_wait_started_condition_lock);
        _wait_started_condition.wait(lck, [& el = _main_el] { return el.is_run(); });
    }
}

void application_impl::application_abort()
{
    SRV_TRACE_SIGNAL(__FUNCTION__);

#if defined(SERVER_LIB_PLATFORM_LINUX)
    ::srv_c_app_abort(0);
#else
    abort();
#endif
}

void application_impl::process_signal(int signal)
{
    if (srv_c_is_fail_signal(signal))
    {
        SRV_TRACE_SIGNAL_HEADER_("Got fail signal ");
        TRACE_SIG_NUMBER(signal);

        bool raw_dump_saved = false;
        if (__get_stdump_file_name())
        {
            raw_dump_saved = emergency::save_raw_stdump_s(__get_stdump_file_name()) > 0;
        }

        if (raw_dump_saved && __get_stdump_file_name())
        {
            __set_signal_data(__signal_data::make_fail_data(signal, __get_stdump_file_name()));
        }
        else
        {
            __set_signal_data(__signal_data::make_fail_data(signal));
        }

#if !defined(SERVER_LIB_PLATFORM_LINUX)
        application_impl::application_abort();
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

            __set_signal_data(__signal_data::make_control_data(control_signal::USR1));
            break;
        case SIGUSR2:

            SRV_TRACE_SIGNAL("Got USR2 control signal");

            __set_signal_data(__signal_data::make_control_data(control_signal::USR2));
            break;
        default:
#else
        {
#endif
            SRV_TRACE_SIGNAL("Got exit signal");

            __set_signal_data(__signal_data::make_exit_data(signal));
        }
    }
}

void application_impl::process_atexit()
{
    SRV_TRACE_SIGNAL(__FUNCTION__);

    if (logger::check_instance())
        logger::instance().lock(); //boost logger implementation failes after 'exit' instructions

    auto& app = instance();
    if (app.is_running())
    {
        SRV_ERROR("std::exit is not allowed to stop application. Use application.stop instead");
    }
}

void application_impl::process_fail()
{
    SRV_TRACE_SIGNAL(__FUNCTION__);

    auto& app = instance();
    if (!app.config()._corefile_fail_thread_only)
    {
        if (logger::check_instance())
            logger::instance().lock(); //some logger configurations can make application suspended in forked thread
    }

    using Callbackdata_type = __signal_data::data_type;

    auto last_signal_data = __get_signal_data();
    if (last_signal_data.type == Callbackdata_type::fail)
    {
        app.process_fail(last_signal_data.data.signo, last_signal_data.path_data);
    }
}

void application_impl::run_impl()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    pthread_t signal_thread_id = 0;
#endif

    std::thread signal_thread([&]() {
        SRV_LOGC_TRACE("Signal thread has just started");

#if defined(SERVER_LIB_PLATFORM_LINUX)
        signal_thread_id = pthread_self();
#endif

        using Callbackdata_type = __signal_data::data_type;

        auto process_handler = [this]() -> bool {
            auto last_signal_data = __get_signal_data();
            switch (last_signal_data.type)
            {
            case Callbackdata_type::exit:
                this->_signal_thread_terminating = true;
                this->process_exit(last_signal_data.data.signo);
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
            srv_c_app_mt_wait_sig_callback(__sig_callback_wrapper);

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
            _exit_callback(0);
    }

    SRV_LOGC_INFO("Application has stopped");
    cleanup();
    auto exit_code = _exit_code.load();
    if (exit_code != 0)
    {
        if (exit_code < 0)
            exit_code = exit_code_error;

        _exit(exit_code);
    }
}

void application_impl::process_fail(const int signo, const char* dump_file_path)
{
    if (_fail_callback)
    {
        SRV_LOGC_TRACE(__FUNCTION__ << "(" << dump_file_path << ")");

        auto fail_callback = _fail_callback;
        fail_callback(signo, dump_file_path);
    }
}

void application_impl::process_exit(const int signo)
{
    if (_main_el.is_running())
    {
        SRV_LOGC_TRACE(__FUNCTION__);

        if (_exit_callback)
        {
            auto exit_callback = _exit_callback;
            _main_el.wait([exit_callback, signo]() {
                exit_callback(signo);
            });
        }

        stop(EXIT_FAILURE_SIG_BASE_ + signo);
    }
}

void application_impl::process_control(control_signal signal)
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

} // namespace server_lib
