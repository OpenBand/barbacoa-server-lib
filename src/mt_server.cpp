#include <server_lib/mt_server.h>
#include <server_lib/logging_helper.h>
#include <server_lib/asserts.h>
#include <server_lib/emergency_helper.h>
#include <server_lib/platform_config.h>

#include <mutex>
#include <condition_variable>
#include <thread>

#include <cstring>

#include <server_clib/server.h>
#if defined(SERVER_LIB_PLATFORM_LINUX)

#include <sys/syscall.h>

#include <signal.h>
#include <pthread.h>
#include <alloca.h>
#endif

namespace server_lib {

namespace impl {
    using sig_callback_ext_type = std::function<void(int)>;

    static sig_callback_ext_type g_sig_callback = nullptr;

    void sig_callback_wrapper(int signo)
    {
        if (g_sig_callback)
            g_sig_callback(signo);
    }

    struct callback_data
    {
        using user_signal = mt_server::user_signal;

        union {
            int signo = 0;
            user_signal signal;
        } data;

        enum class data_type
        {
            null,
            exit,
            usersignal
        };
        data_type type = data_type::null;

        static callback_data make_exit_data(const int signo);
        static callback_data make_config_data(const user_signal);
    };

    callback_data callback_data::make_exit_data(const int signo)
    {
        callback_data r;
        r.type = data_type::exit;
        r.data.signo = signo;
        return r;
    }

    callback_data callback_data::make_config_data(const user_signal sig)
    {
        callback_data r;
        r.type = data_type::usersignal;
        r.data.signal = sig;
        return r;
    }

} // namespace impl

class mt_server_impl
{
public:
    static char crash_dump_file_path[PATH_MAX];

    mt_server_impl() = default;

    void init(bool daemon)
    {
        SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");

        impl::g_sig_callback = [this](int signo) { this->process_signal(signo); };

        server_init_default_signals_should_register();

        if (daemon)
        {
            server_mt_init_daemon(nullptr, impl::sig_callback_wrapper);
        }
        else
        {
            server_mt_init(nullptr, impl::sig_callback_wrapper);
        }
    }

    using exit_callback_type = mt_server::exit_callback_type;
    using fail_callback_type = mt_server::fail_callback_type;
    using control_callback_type = mt_server::control_callback_type;

    int run(main_loop& e,
            exit_callback_type exit_callback,
            fail_callback_type fail_callback,
            control_callback_type control_callback);

    void start(main_loop& e,
               exit_callback_type exit_callback,
               fail_callback_type fail_callback,
               control_callback_type control_callback);

    void stop(main_loop& e)
    {
        SRV_ASSERT(e.is_main(), "Only main loop accepted");

        e.stop();
    }

    void wait_started(main_loop& e, std::function<void(void)>&& start_notify)
    {
        SRV_ASSERT(e.is_main(), "Only main loop accepted");

        while (!e.is_running())
        {
            std::unique_lock<std::mutex> lck(_wait_started_condition_lock);
            _wait_started_condition.wait(lck);
        }
        start_notify();
    }

protected:
    void run_impl(main_loop& e,
                  exit_callback_type exit_callback,
                  fail_callback_type fail_callback,
                  control_callback_type control_callback);

    void process_exit()
    {
        auto process_exit_ = [this]() {
            if (_exit_callback && _e)
            {
                auto exit_callback = _exit_callback;
                _exit_callback = nullptr;
                if (event_loop::is_main_thread())
                {
                    exit_callback();
                }
                else
                {
                    _e->post([exit_callback]() {
                        exit_callback();
                    });
                }
            }
        };

        if (event_loop::is_main_thread())
            process_exit_();
        else
        {
            std::unique_lock<std::mutex> lck(_process_exit_config_lock);
            process_exit_();
        }
    }

    void process_signal(int signal)
    {
#ifndef NDEBUG
        fprintf(stderr, "Got signal %d\n", signal);
#endif
        if (server_is_fail_signal(signal))
        {
#if !defined(STACKTRACE_DISABLED)
            char crash_dump_file_path_[PATH_MAX] = { 0 };
            if (crash_dump_file_path[0])
            {
                // copy to alternative stack area
                std::strncpy(crash_dump_file_path_, crash_dump_file_path, PATH_MAX - 1);
            }

            if (crash_dump_file_path_[0])
            {
                emergency_helper::save_dump(crash_dump_file_path_);

                // it maybe fail here but dump has been saved already
                this->process_fail(crash_dump_file_path_);
            }
            else
#endif //!STACKTRACE_DISABLED
                this->process_fail(nullptr);
        }
        else
        {
#if defined(SERVER_LIB_PLATFORM_LINUX)
            using user_signal = mt_server::user_signal;

            switch (signal)
            {
            case SIGUSR1:
                _callback_data = impl::callback_data::make_config_data(user_signal::USR1);
                break;
            case SIGUSR2:
                _callback_data = impl::callback_data::make_config_data(user_signal::USR2);
                break;
            default:
#else
            {
#endif
                _callback_data = impl::callback_data::make_exit_data(signal);
            }
        }
    }

    void process_fail(const char* dump_file_path)
    {
        auto process_fail_ = [this, dump_file_path]() {
            if (_fail_callback && _e)
            {
                auto fail_callback = _fail_callback;
                _fail_callback = nullptr;
                if (event_loop::is_main_thread())
                {
                    fail_callback(dump_file_path);
                }
                else
                {
                    // pointer dump_file_path should be stay valid in main thread because was created from alternative stack area
                    _e->post([fail_callback, dump_file_path]() { fail_callback(dump_file_path); });
                }
            }
        };

        if (event_loop::is_main_thread())
            process_fail_();
        else
        {
            std::unique_lock<std::mutex> lck(_process_fail_config_lock);
            process_fail_();
        }
    }

private:
    impl::callback_data _callback_data;
    std::thread _wait_synch_signal_thread;
    std::condition_variable _wait_started_condition;
    std::mutex _wait_started_condition_lock;

    main_loop* _e = nullptr;
    std::mutex _process_exit_config_lock;
    exit_callback_type _exit_callback = nullptr;
    std::mutex _process_fail_config_lock;
    fail_callback_type _fail_callback = nullptr;
}; // namespace server_lib

int mt_server_impl::run(main_loop& e,
                        exit_callback_type exit_callback,
                        fail_callback_type fail_callback,
                        control_callback_type control_callback)
{
    if (!event_loop::is_main_thread() || !e.is_main())
        return exit_code_error;

    run_impl(e, exit_callback, fail_callback, control_callback);
    return exit_code_ok;
}

void mt_server_impl::start(main_loop& e,
                           exit_callback_type exit_callback,
                           fail_callback_type fail_callback,
                           control_callback_type control_callback)
{
    SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");
    SRV_ASSERT(e.is_main(), "Only main loop accepted");

    run_impl(e, exit_callback, fail_callback, control_callback);
}

void mt_server_impl::run_impl(main_loop& e,
                              exit_callback_type exit_callback,
                              fail_callback_type fail_callback,
                              control_callback_type control_callback)
{
    _e = &e;

    {
        std::unique_lock<std::mutex> lck(_process_exit_config_lock);
        if (exit_callback)
        {
            _exit_callback = exit_callback;
        }
        else
        {
            _exit_callback = [this]() {
#ifndef NDEBUG
                fprintf(stderr, "Got signal in default exit callback\n");
#endif
                stop(*_e);
            };
        }
    }
    {
        std::unique_lock<std::mutex> lck(_process_fail_config_lock);
        if (fail_callback)
        {
            _fail_callback = [fail_callback](const char* dump_path) {
                fail_callback(dump_path);
                abort();
            };
        }
        else
        {
            _fail_callback = [](const char*) {
#ifndef NDEBUG
                fprintf(stderr, "Got signal in default fail callback\n");
#endif
                abort();
            };
        }
    }

    std::thread wait_thread([this, &e, exit_callback, fail_callback, control_callback]() {
#if defined(SERVER_LIB_PLATFORM_LINUX)
        pthread_setname_np(pthread_self(), "signal");
#endif

        using Callbackdata_type = impl::callback_data::data_type;
        bool terminated = false;
        while (!terminated)
        {
            server_mt_wait_sig_callback(impl::sig_callback_wrapper);
#ifndef NDEBUG
            fprintf(stderr, "Got signal in signal thread\n");
#endif
            if (Callbackdata_type::exit == _callback_data.type)
            {
                terminated = true;
                this->process_exit();
            }
            if (Callbackdata_type::usersignal == _callback_data.type && control_callback)
            {
                auto data = _callback_data.data.signal;
                e.post([control_callback, data]() { control_callback(data); });
            }
        }
        SRV_LOG_TRACE("Signal thread is stopped");
    });
    _wait_synch_signal_thread.swap(wait_thread);

    e.set_exit_callback(_exit_callback);
    e.start([this]() { _wait_started_condition.notify_all(); });
}

char mt_server_impl::crash_dump_file_path[PATH_MAX] = { 0 };

mt_server::mt_server()
{
    SRV_ASSERT(event_loop::is_main_thread(), "Only for main thread allowed");
    _impl = std::make_unique<mt_server_impl>();
}

mt_server::~mt_server()
{
}

void mt_server::init(bool daemon)
{
    _impl->init(daemon);
}

void mt_server::set_crash_dump_file_name(const char* crash_dump_file_path)
{
    std::strncpy(mt_server_impl::crash_dump_file_path, crash_dump_file_path, PATH_MAX - 1);
}

void mt_server::start(main_loop& e,
                      exit_callback_type exit_callback,
                      fail_callback_type fail_callback,
                      control_callback_type control_callback)
{
    _impl->start(e, exit_callback, fail_callback, control_callback);
}

int mt_server::run(main_loop& e,
                   exit_callback_type exit_callback,
                   fail_callback_type fail_callback,
                   control_callback_type control_callback)
{
    return _impl->run(e, exit_callback, fail_callback, control_callback);
}

void mt_server::stop(main_loop& e)
{
    _impl->stop(e);
}

void mt_server::wait_started(main_loop& e, std::function<void(void)>&& start_notify)
{
    _impl->wait_started(e, std::forward<std::function<void(void)>>(start_notify));
}

} // namespace server_lib
