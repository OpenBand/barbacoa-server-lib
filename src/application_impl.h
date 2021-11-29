#pragma once

#include <server_lib/application.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace server_lib {

class application_impl
{
    friend class __internal_application_manager;

protected:
    application_impl();

public:
    static application_impl& instance();
    static application& app_instance();

    const application_config& config() const
    {
        return _config;
    }

    void set_config(const application_config& config);

    void init();

    using start_callback_type = application::start_callback_type;
    using exit_callback_type = application::exit_callback_type;
    using fail_callback_type = application::fail_callback_type;
    using control_callback_type = application::control_callback_type;

    int run();

    bool is_running() const
    {
        return _main_el.is_running();
    }

    void on_start(start_callback_type&& callback);

    void on_exit(exit_callback_type&& callback);

    void on_fail(fail_callback_type&& callback);

    void on_control(control_callback_type&& callback);

    void wait();

    application_main_loop& loop()
    {
        return _main_el;
    }

    void stop(int exit_code)
    {
        _exit_code = exit_code;
        _main_el.stop(); // makes 'run' exit
    }

private:
    static void application_abort();

    using control_signal = application::control_signal;

    static void process_signal(int signal);
    static void process_atexit();
    static void process_fail();

    void run_impl();

    void process_fail(const int signo, const char* dump_file_path);

    void process_exit(const int signo);

    void process_control(control_signal signal);

    void cleanup()
    {
        _signal_thread_initiated = false;
        _signal_thread_terminating = false;
    }

private:
    application_config _config;
    application_main_loop _main_el;
    std::thread _signal_thread;
    std::atomic_bool _signal_thread_initiated;
    std::atomic_bool _signal_thread_terminating; // if it is start terminating at least
    std::atomic_int _exit_code;

    std::condition_variable _wait_started_condition;
    std::mutex _wait_started_condition_lock;

    start_callback_type _start_callback = nullptr;
    exit_callback_type _exit_callback = nullptr;
    fail_callback_type _fail_callback = nullptr;
    control_callback_type _control_callback = nullptr;
};

} // namespace server_lib
