#include <server_lib/mt_event_loop.h>
#include <server_lib/asserts.h>
#include <server_lib/thread_sync_helpers.h>

#include <boost/utility/in_place_factory.hpp>

#include "logger_set_internal_group.h"

namespace server_lib {

mt_event_loop::mt_event_loop(uint8_t nb_threads)
    : event_loop(true)
    , _nb_additional_threads(nb_threads > 0 ? nb_threads - 1 : 0)
{
    SRV_ASSERT(nb_threads > 0, "Threads are required");

    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_ << " in thread pool " << this->nb_threads());

    _waiting_threads = 0;
    _thread_ids.reserve(this->nb_threads());
}

event_loop& mt_event_loop::start()
{
    if (is_running())
        return *this;

    SRV_LOGC_INFO(SRV_FUNCTION_NAME_);

    try
    {
        SRV_ASSERT(_run_in_separate_thread);

        _is_running = true;

        // The 'work' object guarantees that 'run()' function
        // will not exit while work is underway, and that it does exit
        // when there is no unfinished work remaining.
        // It is quite like infinite loop (in CCU safe manner of course)
        _loop_maintainer = boost::in_place(std::ref(*_pservice));

        _waiting_threads = nb_threads();

        SRV_LOGC_TRACE("Event loop is starting and waiting for " << this->_waiting_threads << " threads");

        post([this]() {
            size_t waiting_threads = 0;
            while (!this->_waiting_threads.compare_exchange_weak(waiting_threads, 0))
            {
                if (!waiting_threads)
                    return;

                waiting_threads = 0;
                spin_loop_pause();
            }

            SRV_LOGC_TRACE("Event loop has started");

            _is_run = true;

            notify_start();
        });

        _thread.reset(new std::thread([this]() {
            {
                std::lock_guard<std::mutex> lock(_thread_ids_mutex);
                _thread_ids.emplace_back(std::this_thread::get_id());
            }
            std::atomic_fetch_sub<size_t>(&_waiting_threads, 1);
            run();
        }));

        for (size_t ci = 0; ci < _nb_additional_threads; ++ci)
        {
            _additional_threads.emplace_back(new std::thread([this]() {
                {
                    std::lock_guard<std::mutex> lock(_thread_ids_mutex);
                    _thread_ids.emplace_back(std::this_thread::get_id());
                }
                std::atomic_fetch_sub<size_t>(&_waiting_threads, 1);
                run();
            }));
        }
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());
        stop();
        throw;
    }

    return *this;
}

void mt_event_loop::stop()
{
    if (!is_running())
        return;

    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

    try
    {
        _is_running = false;

        _waiting_threads = 0;
        _loop_maintainer = boost::none; // finishing 'infinite loop'
        // It will stop all threads (exit from all 'run()' functions)
        _pservice->stop();

        if (_thread && _thread->joinable())
        {
            _thread->join();
        }

        for (auto&& thread : _additional_threads)
        {
            thread->join();
        }
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());

        SRV_THROW();
    }

    _thread_ids.clear();
    _additional_threads.clear();
    reset();
}

bool mt_event_loop::is_this_loop() const
{
    std::lock_guard<std::mutex> lock(_thread_ids_mutex);
    for (auto&& thread_id : _thread_ids)
    {
        if (thread_id == std::this_thread::get_id())
            return true;
    }
    return false;
}

} // namespace server_lib
