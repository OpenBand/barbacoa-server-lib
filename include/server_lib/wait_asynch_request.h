#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

namespace server_lib {

template <typename Result, typename CallerFunc, typename AsynchFunc>
Result wait_preliminary_async_call(const Result initial_result, CallerFunc&& caller_func, AsynchFunc&& asynch_func, int32_t timeout_ms = -1)
{
    std::condition_variable _donecheck;
    std::mutex _cond_data_guard;
    Result result = initial_result;
    bool _done_request = false;

    std::unique_lock<std::mutex> lck(_cond_data_guard); //guard _done_request and result variables

    auto _asynch = [&asynch_func, &result, &_done_request, &_cond_data_guard, &_donecheck]() {
        std::unique_lock<std::mutex> lck(_cond_data_guard);

        result = asynch_func();
        _done_request = true;
        lck.unlock();
        _donecheck.notify_one(); //internally lock _cond_data_guard
    };

    result = caller_func(_asynch);

    while (result == initial_result && !_done_request) //for OS interruptions case
    {
        auto done = [&_done_request]() {
            return _done_request;
        };

        if (timeout_ms > 0)
        {
            if (!_donecheck.wait_for(lck, std::chrono::milliseconds(timeout_ms), done))
                break;
        }
        else
        {
            _donecheck.wait(lck, done); //internally unlock _cond_data_guard
        }
    }

    return result;
}

template <typename Result, typename CallerFunc, typename AsynchFunc>
Result wait_async_call(const Result initial_result, CallerFunc&& caller_func, AsynchFunc&& asynch_func, int32_t timeout_ms = -1)
{
    // ignore preliminary check
    auto caller_func_wrapper = [&initial_result, &caller_func](auto asynch_func) -> Result {
        caller_func(asynch_func);
        return initial_result;
    };
    return wait_preliminary_async_call(initial_result, caller_func_wrapper, asynch_func, timeout_ms);
}

} // namespace server_lib
