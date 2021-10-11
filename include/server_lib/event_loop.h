#pragma once

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <thread>
#include <atomic>

#include <server_lib/types.h>
#include <server_lib/timers.h>
#include <server_lib/asserts.h>

#include "wait_asynch_request.h"

namespace server_lib {

DECLARE_PTR(event_loop);

/**
 * \ingroup common
 *
 * \brief This class provides minimal thread management with message queue
 * It is crucial class for multythread applications based on server_lib!
 */
class event_loop
{
public:
    using timer = server_lib::timer<event_loop>;
    using periodical_timer = server_lib::periodical_timer<event_loop>;

    event_loop(bool in_separate_thread = true);
    virtual ~event_loop();

    operator boost::asio::io_service&()
    {
        SRV_ASSERT(_pservice);
        return *_pservice;
    }

    std::shared_ptr<boost::asio::io_service> service() const
    {
        return _pservice;
    }

    event_loop& change_thread_name(const std::string&);

    /**
     * Start loop
     *
     */
    virtual void start();

    /**
     * Stop loop
     *
     */
    virtual void stop();

    using callback_type = std::function<void(void)>;

    /**
     * \brief Invoke callback when loop will start
     *
     * \param callback
     *
     * \return loop object
     *
     */
    event_loop& on_start(callback_type&& callback);

    /**
     * \brief Invoke callback when loop will stop
     *
     * \param callback
     *
     * \return loop object
     *
     */
    event_loop& on_stop(callback_type&& callback);

    /**
     * Start has just called but loop probably couldn't run yet
     */
    bool is_running() const
    {
        return _is_running;
    }

    /**
     * Loop has run
     */
    bool is_run() const
    {
        return _is_run;
    }

    static bool is_main_thread();

    auto queue_size() const
    {
        return _queue_size.load();
    }

    bool is_this_loop() const
    {
        return _id.load() == std::this_thread::get_id();
    }

    long native_thread_id() const
    {
        return _native_thread_id.load();
    }

    /**
     * Callback that is invoked in thread owned by this event_loop
     * Multiple invokes are executed in queue. Post can be called
     * before or after starting. Callback wiil be invoked in 'run'
     * state
     *
     * \param handler
     *
     * \return loop object
     *
     */
    template <typename Handler>
    event_loop& post(Handler&& handler)
    {
        SRV_ASSERT(_pservice);
        auto handler_ = [pqueue_size = &_queue_size, handler = std::move(handler)]() mutable {
            handler();
            std::atomic_fetch_sub<uint64_t>(pqueue_size, 1);
        };
        std::atomic_fetch_add<uint64_t>(&_queue_size, 1);

        //-------------POST(WRAP(...)) - design explanation:
        //
        //* The 'post' guarantees that the handler will only be called in a thread
        //  in which the 'run()'
        //* The 'strand' object guarantees that all 'post's are executed in queue
        //
        _pservice->post(_strand.wrap(std::move(handler_)));

        return *this;
    }

    /**
     * Waiting for callback with result endlessly
     *
     * \param initial_result - Result for case when callback can't be invoked
     * \param asynch_func - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename Result, typename AsynchFunc>
    Result wait_result(Result&& initial_result, AsynchFunc&& asynch_func)
    {
        auto call = [this](auto asynch_func) {
            this->post(asynch_func);
        };
        return wait_async_result(std::forward<Result>(initial_result), call, asynch_func);
    }

    /**
     * Waiting for callback with result
     *
     * \param initial_result - Result for case when callback can't be invoked
     * \param asynch_func - Callback that is invoked in thread owned by this event_loop
     * \param duration - Timeout (std::chrono::duration type)
     *
     */
    template <typename Result, typename AsynchFunc, typename DurationType>
    Result wait_result(Result&& initial_result, AsynchFunc&& asynch_func, DurationType&& duration)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

        auto call = [this](auto asynch_func) {
            this->post(asynch_func);
        };
        return wait_async_result(std::forward<Result>(initial_result), call, asynch_func, ms.count());
    }

    /**
     * Waiting for callback without result (void) endlessly
     *
     * \param asynch_func - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename AsynchFunc>
    void wait(AsynchFunc&& asynch_func)
    {
        auto call = [this](auto asynch_func) {
            this->post(asynch_func);
        };
        wait_async(call, asynch_func);
    }

    /**
     * Waiting for callback without result (void)
     *
     * \param asynch_func - Callback that is invoked in thread owned by this event_loop
     * only before this function return
     * \param duration - Timeout (std::chrono::duration type)
     *
     * \return bool - if callback was invoked before duration expiration
     *
     */
    template <typename AsynchFunc, typename DurationType>
    bool wait(AsynchFunc&& asynch_func, DurationType&& duration)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

        auto call = [this](auto asynch_func) {
            this->post(asynch_func);
        };
        return wait_async(call, asynch_func, ms.count());
    }

    /**
     * \brief Waiting for finish for starting procedure
     * or immediately (if event loop has already started)
     * return
     *
     */
    void wait();

    /**
     * Callback that is invoked after certain timeout
     *
     * \param duration - Timeout (std::chrono::duration type)
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     * \return loop object
     *
     */
    template <typename DurationType, typename Handler>
    event_loop& start_timer(DurationType&& duration, Handler&& callback)
    {
        SRV_ASSERT(_pservice);

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum timer accuracy");

        auto timer = std::make_shared<boost::asio::deadline_timer>(*_pservice);

        timer->expires_from_now(boost::posix_time::milliseconds(ms.count()));
        timer->async_wait(_strand.wrap([timer /*save timer object*/, callback](const boost::system::error_code& ec) {
            if (!ec)
            {
                callback();
            }
        }));
        return *this;
    }

protected:
    void run();

protected:
    const bool _run_in_separate_thread = false;
    std::atomic_bool _is_running;
    std::atomic_bool _is_run;
    callback_type _start_callback = nullptr;
    callback_type _stop_callback = nullptr;
    std::atomic<std::thread::id> _id;
    std::atomic_long _native_thread_id;
    std::shared_ptr<boost::asio::io_service> _pservice;
    boost::asio::io_service::strand _strand;
    boost::optional<boost::asio::io_service::work> _loop_maintainer;
    std::unique_ptr<std::thread> _thread;
    std::string _thread_name = "io_service loop";
    std::atomic_uint64_t _queue_size;

private:
    void apply_thread_name();
};

} // namespace server_lib
