#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <atomic>

namespace server_lib {

template <typename Result, typename CallerFunc, typename AsynchFunc>
Result wait_async_call(const Result initial_result, CallerFunc&& caller_func, AsynchFunc&& asynch_func, int32_t timeout_ms = -1)
{
    std::condition_variable donecheck;
    std::mutex cond_data_guard;

    using context_guard_type = std::atomic<int8_t>;
    context_guard_type* pcontext_guard = nullptr;

    std::unique_lock<std::mutex> lck(cond_data_guard); //guard for result and done_request variables
    Result result = initial_result;
    bool done_request = false;

    if (timeout_ms > 0)
    {
        //This one protects from async function calling after current context destroying
        //It should be appropriate context_check calls = [amount - 1]
        pcontext_guard = new context_guard_type(1);
    }
    auto context_check = [p = pcontext_guard]() -> bool {
        if (!p)
            return true;

        auto left = std::atomic_fetch_sub<context_guard_type::value_type>(p, 1);
        if (left < 1)
        {
            delete p;
            return false; //all context data is invalid already
        }
        return true;
    };
    auto asynch_func_wrapper = [context_check,
                                asynch_func,
                                &result,
                                &done_request,
                                &cond_data_guard,
                                &donecheck]() {
        //there is not mutex to halt immediately by timeout in condition variable
        auto asynch_result = asynch_func();
        if (!context_check())
            return;

        std::unique_lock<std::mutex> lck(cond_data_guard);
        result = std::move(asynch_result);
        done_request = true;
        donecheck.notify_one(); //internally lock cond_data_guard
    };

    caller_func(asynch_func_wrapper);

    auto done = [&done_request]() {
        return done_request;
    };

    while (!done()) //for OS interruptions case
    {
        if (timeout_ms > 0)
        {
            //https://linux.die.net/man/3/pthread_cond_timedwait
            if (donecheck.wait_for(lck, std::chrono::milliseconds(timeout_ms))
                == std::cv_status::timeout)
                break;
        }
        else
        {
            donecheck.wait(lck, done);
        }
    }

    context_check();
    return result;
}

} // namespace server_lib
