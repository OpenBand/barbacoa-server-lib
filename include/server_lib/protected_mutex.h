#pragma once

#include <server_lib/asserts.h>

#include <thread>
#include <mutex>
#include <functional>

namespace server_lib {
//The type meet the BasicLockable requirements:
//  https://en.cppreference.com/w/cpp/named_req/BasicLockable
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
        while (!_ows.compare_exchange_weak(et_, et))
            ;
    }
    void unlock()
    {
        size_t et = std::hash<std::thread::id> {}(std::this_thread::get_id());
        auto et_ = et;
        SRV_ASSERT(_ows.compare_exchange_strong(et_, et), "Not owner try to reunlock");
        _mutex.unlock();
        while (!_ows.compare_exchange_weak(et_, 0))
            ;
    }

private:
    std::atomic<size_t> _ows;
    Mutex _mutex;
};
} // namespace server_lib
