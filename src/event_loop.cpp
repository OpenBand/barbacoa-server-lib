#include <server_lib/event_loop.h>
#include <server_lib/asserts.h>

#include <boost/utility/in_place_factory.hpp>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <pthread.h>
#include <sys/syscall.h>
#endif

#include "logger_set_internal_group.h"

namespace server_lib {

#if !defined(SERVER_LIB_PLATFORM_LINUX)
static std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();
#endif

event_loop::event_loop(bool in_separate_thread /*= true*/)
    : _run_in_separate_thread(in_separate_thread)
{
    if (_run_in_separate_thread)
    {
        SRV_LOGC_TRACE(SRV_FUNCTION_NAME_ << " in separate thread");
    }
    else
    {
        SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);
    }

    reset();
}

event_loop::~event_loop()
{
    try
    {
        if (_run_in_separate_thread)
        {
            SRV_LOGC_TRACE(SRV_FUNCTION_NAME_ << " in separate thread");
        }
        else
        {
            SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);
        }

        stop();
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());
    }
}

void event_loop::reset()
{
    _is_running = false;
    _is_run = false;
    _start_callback = nullptr;
    _stop_callback = nullptr;
    _id = std::this_thread::get_id();
    _native_thread_id = 0l;

    _pservice = std::make_shared<boost::asio::io_service>();
    _pstrand = std::make_unique<boost::asio::io_service::strand>(*_pservice);
    _queue_size = 0;
    if (_run_in_separate_thread)
        _thread.reset();
}

void event_loop::apply_thread_name()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    SRV_ASSERT(0 == pthread_setname_np(pthread_self(), _thread_name.c_str()));
#endif
}

bool event_loop::is_main_thread()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    return getpid() == syscall(SYS_gettid);
#else
    return std::this_thread::get_id() == MAIN_THREAD_ID;
#endif
}

event_loop& event_loop::change_thread_name(const std::string& name)
{
    static int MAX_THREAD_NAME_SZ = 15;
    if (name.length() > MAX_THREAD_NAME_SZ)
        _thread_name = name.substr(0, MAX_THREAD_NAME_SZ);
    else
        _thread_name = name;
    if (!_run_in_separate_thread)
    {
        apply_thread_name();
    }
    else if (is_running())
    {
        post([this]() {
            apply_thread_name();
        });
    }
    return *this;
}

void event_loop::start()
{
    if (is_running())
        return;

    SRV_LOGC_INFO(SRV_FUNCTION_NAME_);

    try
    {
        _is_running = true;

        // The 'work' object guarantees that 'run()' function
        // will not exit while work is underway, and that it does exit
        // when there is no unfinished work remaining.
        // It is quite like infinite loop (in CCU safe manner of course)
        _loop_maintainer = boost::in_place(std::ref(*_pservice));

        post([this]() {
            SRV_LOGC_TRACE("Event loop has started");

            _is_run = true;

            if (_start_callback)
                _start_callback();
        });

        if (_run_in_separate_thread)
        {
            _thread.reset(new std::thread([this]() {
#if defined(SERVER_LIB_PLATFORM_LINUX)
                _native_thread_id = syscall(SYS_gettid);
#elif defined(SERVER_LIB_PLATFORM_WINDOWS)
                _native_thread_id = ::GetCurrentThreadId();
#else
                _native_thread_id = static_cast<long>(std::this_thread::get_id().native_handle());
#endif
                run();
            }));
        }
        else
        {
            run();
        }
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());
        stop();
        throw;
    }
}

void event_loop::stop()
{
    if (!is_running())
        return;

    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

    try
    {
        _is_running = false;

        _loop_maintainer = boost::none; //finishing 'infinite loop'
        _pservice->stop();

        if (_thread && _thread->joinable())
        {
            _thread->join();
        }
    }
    catch (const std::exception& e)
    {
        SRV_THROW();
    }

    reset();
}

event_loop& event_loop::on_start(callback_type&& callback)
{
    _start_callback = callback;
    return *this;
}

event_loop& event_loop::on_stop(callback_type&& callback)
{
    _stop_callback = callback;
    return *this;
}

void event_loop::wait()
{
    if (!is_running())
    {
        wait([]() {});
    }
}

void event_loop::run()
{
    _id = std::this_thread::get_id();
    try
    {
        apply_thread_name();

        SRV_LOGC_TRACE("Event loop is starting");

        _pservice->run();

        SRV_LOGC_TRACE("Event loop has stopped");

        if (_stop_callback)
            _stop_callback();
    }
    catch (const std::exception& e)
    {
        SRV_THROW();
    }
}

} // namespace server_lib
