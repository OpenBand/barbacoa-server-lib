#pragma once

#include "thread_sync_helpers.h"

#include <server_lib/asserts.h>

#include <thread>
#include <mutex>
#include <functional>

namespace server_lib {

/**
 * \ingroup common
 *
 * This class protects from undefined behavior if mutex lock/unlock functions are called improperly.
 * It assumes exclusive and none recursive mutex.
 * It throws exceptions instead undefined behavior.
 *
 * The type meet the BasicLockable requirements to wrap C++ mutex:
 *  https://en.cppreference.com/w/cpp/named_req/BasicLockable
 */
template <class Mutex>
class protected_mutex
{
public:
    protected_mutex()
        : _ows(0)
    {
    }
    protected_mutex(const protected_mutex&) = delete;

    void lock()
    {
        size_t et = std::hash<std::thread::id> {}(std::this_thread::get_id());
        auto et_ = et;
        SRV_ASSERT(!_ows.compare_exchange_strong(et_, et), "Owner try to relock");
        _mutex.lock();
        _ows.store(et);
    }
    void unlock()
    {
        size_t et = std::hash<std::thread::id> {}(std::this_thread::get_id());
        auto et_ = et;
        bool check_result = _ows.compare_exchange_strong(et_, et);
        if (!check_result)
        {
            SRV_ASSERT(!et_, "Attempt to unlock not locked mutex");
            SRV_ASSERT(et_ > 0, "Not owner try to unlock");
        }
        _mutex.unlock();
        while (!_ows.compare_exchange_weak(et_, 0))
            spin_loop_pause();
    }

private:
    std::atomic<size_t> _ows;
    Mutex _mutex;
};
} // namespace server_lib
