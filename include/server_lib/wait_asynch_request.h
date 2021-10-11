#pragma once

#include <server_lib/asserts.h>

#include <functional>
#include <mutex>
#include <chrono>
#include <future>
#include <memory>

namespace server_lib {

/**
 * \ingroup common
 *
 * Waiting for asynch_func callback result by means of
 * caller_func. There is timeout if timeout_ms  > 0
 *--------------------------------------------------------------
 * For example if there is function or class method looks like:
 *
 * caller_func(asynch_func -> Result) -> void (ignored)
 *
 * It waits asynch_func was called and return it's result or initial_result
 * if timeout was
 *
 * \param initial_result - Result that suppose to be if asynch_func was not called
 * \param caller_func
 * \param asynch_func - Callback that is invoked by caller_func
 * \param timeout_ms - Timeout (std::chrono::duration type)
 *
 * \return Result
 *
 */
template <typename Result, typename CallerFunc, typename AsynchFunc>
Result wait_async_result(Result&& initial_result, CallerFunc&& caller_func, AsynchFunc&& asynch_func, int32_t timeout_ms = -1)
{
    Result result = std::forward<Result>(initial_result);

    try
    {
        std::promise<bool> wait_ctx;
        if (timeout_ms > 0)
        {
            auto pguard = std::make_shared<std::mutex>();
            auto asynch_func_wrapper = [pguard_ = std::weak_ptr<std::mutex>(pguard),
                                        asynch_func,
                                        &result,
                                        &wait_ctx]() {
                auto pguard = pguard_.lock();
                if (pguard)
                {
                    try
                    {
                        auto result_ = asynch_func();
                        std::lock_guard<std::mutex> lck(*pguard);
                        if (pguard.use_count() > 1)
                        {
                            result = std::move(result_);
                            wait_ctx.set_value(true);
                        } //else timeout was arisen
                    }
                    catch (...)
                    {
                        try
                        {
                            std::lock_guard<std::mutex> lck(*pguard);
                            if (pguard.use_count() > 1)
                            {
                                wait_ctx.set_exception(std::current_exception());
                            } //else:
                            //        timeout was arisen
                            //        and caller doesn't wait any exception too
                        }
                        catch (...)
                        {
                        }
                    }
                } //else event queue was overloaded
            };

            caller_func(asynch_func_wrapper);

            auto f = wait_ctx.get_future();
            if (std::future_status::ready == f.wait_for(std::chrono::milliseconds(timeout_ms)))
                f.get();
            std::unique_lock<std::mutex> lck(*pguard);
            return result;
        }
        else
        {
            auto asynch_func_wrapper = [&asynch_func,
                                        &result,
                                        &wait_ctx]() {
                try
                {
                    result = asynch_func();
                    wait_ctx.set_value(true);
                }
                catch (...)
                {
                    try
                    {
                        wait_ctx.set_exception(std::current_exception());
                    }
                    catch (...)
                    {
                    }
                }
            };

            caller_func(asynch_func_wrapper);

            wait_ctx.get_future().get();
        }
    }
    catch (const std::exception& e)
    {
        SRV_ERROR(e.what());
    }

    return result;
}

/**
 * \ingroup common
 *
 * Waiting for asynch_func callback by means of
 * caller_func. There is timeout if timeout_ms  > 0
 *--------------------------------------------------------------
 * For example if there is function or class method looks like:
 *
 * caller_func(asynch_func -> void) -> void (ignored)
 *
 * It waits asynch_func was called and return 'true' or 'false'
 * if timeout was
 *
 * \param caller_func
 * \param asynch_func - Callback that is invoked by caller_func
 * \param timeout_ms - Timeout (std::chrono::duration type)
 *
 * \return bool - if callback was invoked before duration expiration
 *
 */
template <typename CallerFunc, typename AsynchFunc>
bool wait_async(CallerFunc&& caller_func, AsynchFunc&& asynch_func, int32_t timeout_ms = -1)
{
    try
    {
        std::promise<bool> wait_ctx;
        if (timeout_ms > 0)
        {
            std::atomic<bool> complete;
            complete = false;

            auto pguard = std::make_shared<std::mutex>();
            std::promise<bool> wait_ctx;
            auto asynch_func_wrapper = [pguard_ = std::weak_ptr<std::mutex>(pguard),
                                        asynch_func,
                                        &complete,
                                        &wait_ctx]() {
                auto pguard = pguard_.lock();
                if (pguard)
                {
                    try
                    {
                        asynch_func();
                        std::lock_guard<std::mutex> lck(*pguard);
                        if (pguard.use_count() > 1)
                        {
                            complete = true;
                            wait_ctx.set_value(true);
                        } //else timeout was arisen
                    }
                    catch (...)
                    {
                        try
                        {
                            std::lock_guard<std::mutex> lck(*pguard);
                            if (pguard.use_count() > 1)
                            {
                                wait_ctx.set_exception(std::current_exception());
                            } //else:
                            //        timeout was arisen
                            //        and caller doesn't wait any exception too
                        }
                        catch (...)
                        {
                        }
                    }
                } //else event queue was overloaded
            };

            caller_func(asynch_func_wrapper);

            auto f = wait_ctx.get_future();
            if (std::future_status::ready == f.wait_for(std::chrono::milliseconds(timeout_ms)))
                f.get();
            std::unique_lock<std::mutex> lck(*pguard);
            return complete.load();
        }
        else
        {
            auto asynch_func_wrapper = [&asynch_func,
                                        &wait_ctx]() {
                try
                {
                    asynch_func();
                    wait_ctx.set_value(true);
                }
                catch (...)
                {
                    try
                    {
                        wait_ctx.set_exception(std::current_exception());
                    }
                    catch (...)
                    {
                    }
                }
            };

            caller_func(asynch_func_wrapper);

            wait_ctx.get_future().get();
        }
    }
    catch (const std::exception& e)
    {
        SRV_ERROR(e.what());
    }

    return true;
}

} // namespace server_lib
