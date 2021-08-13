#include <server_lib/event_loop.h>
#include <server_lib/logging_helper.h>
#include <server_lib/asserts.h>

#include <boost/utility/in_place_factory.hpp>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <pthread.h>
#include <sys/syscall.h>
#endif

#ifdef SRV_LOG_CONTEXT_
#undef SRV_LOG_CONTEXT_
#endif // #ifdef SRV_LOG_CONTEXT_

#define SRV_LOG_CONTEXT_ "el>" << ((_tid > 0) ? (_tid) : 0) << ((_tid > 0) ? ": " : "| ")

namespace server_lib {

#if !defined(SERVER_LIB_PLATFORM_LINUX)
static std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();
#endif

event_loop::event_loop(bool in_separate_thread /*= true*/)
    : _run_in_separate_thread(in_separate_thread)
    , _pservice(std::make_shared<boost::asio::io_service>())
    , _strand(*_pservice)
    , _queue_size(0)
    , _id(std::this_thread::get_id())
{
    if (_run_in_separate_thread)
    {
        SRV_LOGC_TRACE(SRV_FUNCTION_NAME_ << " in separate thread");
    }
    else
    {
        SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);
    }

    _is_running.store(false);
    _is_main.store(is_main_loop());
}
event_loop::~event_loop()
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

bool event_loop::is_main_thread()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    return getpid() == syscall(SYS_gettid);
#else
    return std::this_thread::get_id() == MAIN_THREAD_ID;
#endif
}

bool event_loop::is_main_loop()
{
    return !_run_in_separate_thread && is_main_thread();
}

void event_loop::apply_thread_name()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    SRV_ASSERT(0 == pthread_setname_np(pthread_self(), _thread_name.c_str()));
#endif
}

void event_loop::change_thread_name(const std::string& name)
{
    static int MAX_THREAD_NAME_SZ = 15;
    if (name.length() > MAX_THREAD_NAME_SZ)
        _thread_name = name.substr(0, MAX_THREAD_NAME_SZ);
    else
        _thread_name = name;
    if (_is_running.load())
    {
        apply_thread_name();
    }
}

void event_loop::start(std::function<void(void)> start_notify, std::function<void(void)> stop_notify)
{
    if (is_running())
        return;

    SRV_LOGC_INFO(SRV_FUNCTION_NAME_);

    _is_main.store(is_main_loop());

    // The 'work' object guarantees that 'run()' function
    // will not exit while work is underway, and that it does exit
    // when there is no unfinished work remaining.
    // It is quite like infinite loop (in CCU safe manner of course)
    _loop_maintainer = boost::in_place(std::ref(*_pservice));

    post([this, start_notify]() {
        SRV_LOGC_TRACE("Event loop is started");

        _is_running.store(true);
        if (start_notify)
            start_notify();
    });

    if (_run_in_separate_thread)
    {
        _thread.reset(new std::thread([this, stop_notify]() {
#if defined(SERVER_LIB_PLATFORM_LINUX)
            _tid = syscall(SYS_gettid);
#elif defined(SERVER_LIB_PLATFORM_WINDOWS)
            _tid = ::GetCurrentThreadId();
#else
            _tid = static_cast<long>(std::this_thread::get_id().native_handle());
#endif

            SRV_LOGC_TRACE("Event loop is starting");

            run();

            SRV_LOGC_TRACE("Event loop is stopped");

            if (stop_notify)
                stop_notify();
        }));
    }
    else
    {
        SRV_LOGC_TRACE("Event loop is starting");

        run();

        SRV_LOGC_TRACE("Event loop is stopped");

        if (stop_notify)
            stop_notify();
    }
}

void event_loop::stop()
{
    if (!is_running())
        return;

    SRV_LOGC_INFO(SRV_FUNCTION_NAME_);

    _loop_maintainer = boost::none; //finishing 'infinite loop'
    _pservice->stop();

    if (_thread && _thread->joinable())
        _thread->join();
    _thread.reset();

    _is_running.store(false);
}

void event_loop::run()
{
    _id.store(std::this_thread::get_id());
    apply_thread_name();

    try
    {
        _pservice->run();
    }
    catch (const std::exception& e)
    {
        // Deal with exception as appropriate.
        SRV_LOGC_ERROR("Catched unexpected exception: " << e.what());
        throw;
    }
    catch (...)
    {
        SRV_LOGC_ERROR("Unknown exception catched");
        throw;
    }
}

main_loop::main_loop(const std::string& name)
    : event_loop(false)
{
    SRV_ASSERT(is_main(), "Invalid main loop creation");

    if (!name.empty())
        change_thread_name(name);
}

void main_loop::set_exit_callback(std::function<void(void)> exit_callback)
{
    _exit_callback = exit_callback;
}

void main_loop::exit(const int exit_code)
{
#ifndef NDEBUG
    fprintf(stderr, "POST exit main loop with code %d\n", exit_code);
#endif
    if (is_running())
    {
        post([this, exit_code]() {
#ifndef NDEBUG
            fprintf(stderr, "Exit main loop with code %d\n", exit_code);
#endif
            if (_exit_callback)
            {
                _exit_callback();
            }
            _exit(exit_code);
        });
    }
    else
    {
        if (_exit_callback)
            _exit_callback();
        _exit(exit_code);
    }
}

void main_loop::start(std::function<void(void)> start_notify, std::function<void(void)> stop_notify)
{
    SRV_ASSERT(is_main_thread(), "Only for main thread allowed");

    SRV_LOGC_TRACE("MAIN loop is started");

    event_loop::start(start_notify, stop_notify);
}

void main_loop::stop()
{
    SRV_ASSERT(is_main_thread(), "Only for main thread allowed");

    event_loop::stop();
}

} // namespace server_lib
