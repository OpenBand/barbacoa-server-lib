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
    using callback_type = std::function<void(void)>;

private:
    template <typename Handler>
    callback_type register_queue(Handler&& callback)
    {
        SRV_ASSERT(_pservice);
        auto callback_ = [this, callback = std::move(callback)]() mutable {
            callback();
            std::atomic_fetch_sub<uint64_t>(&this->_queue_size, 1);
        };
        std::atomic_fetch_add<uint64_t>(&_queue_size, 1);
        return callback_;
    }

    void reset();
    void apply_thread_name();

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
     * \param callback
     *
     * \return loop object
     *
     */
    template <typename Handler>
    event_loop& post(Handler&& callback)
    {
        SRV_ASSERT(_pservice);
        SRV_ASSERT(_pstrand);

        auto callback_ = register_queue(callback);

        //-------------POST(WRAP(...)) - design explanation:
        //
        //* The 'post' guarantees that the callback will only be called in a thread
        //  in which the 'run()'
        //* The 'strand' object guarantees that all 'post's are executed in queue
        //
        _pservice->post(_pstrand->wrap(std::move(callback_)));

        return *this;
    }

    /**
     * Callback that is invoked after certain timeout
     * in thread owned by this event_loop
     *
     * \param duration - Timeout (std::chrono::duration type)
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     * \return loop object
     *
     */
    template <typename DurationType, typename Handler>
    event_loop& post(DurationType&& duration, Handler&& callback)
    {
        SRV_ASSERT(_pservice);
        SRV_ASSERT(_pstrand);

        auto callback_ = register_queue(callback);

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum timer accuracy");

        auto timer = std::make_shared<boost::asio::deadline_timer>(*_pservice);

        timer->expires_from_now(boost::posix_time::milliseconds(ms.count()));
        timer->async_wait(_pstrand->wrap([timer /*save timer object*/, callback_](const boost::system::error_code& ec) {
            if (!ec)
            {
                callback_();
            }
        }));
        return *this;
    }

    /**
     * Waiting for callback with result endlessly
     *
     * \param initial_result - Result for case when callback can't be invoked
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename Result, typename AsynchFunc>
    Result wait_result(Result&& initial_result, AsynchFunc&& callback)
    {
        auto call = [this](auto callback) {
            this->post(callback);
        };
        return wait_async_result(std::forward<Result>(initial_result), call, callback);
    }

    /**
     * Waiting for callback with result
     *
     * \param initial_result - Result for case when callback can't be invoked
     * \param callback - Callback that is invoked in thread owned by this event_loop
     * \param duration - Timeout (std::chrono::duration type)
     *
     */
    template <typename Result, typename AsynchFunc, typename DurationType>
    Result wait_result(Result&& initial_result, AsynchFunc&& callback, DurationType&& duration)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

        auto call = [this](auto callback) {
            this->post(callback);
        };
        return wait_async_result(std::forward<Result>(initial_result), call, callback, ms.count());
    }

    /**
     * Waiting for callback without result (void) endlessly
     *
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename AsynchFunc>
    void wait(AsynchFunc&& callback)
    {
        auto call = [this](auto callback) {
            this->post(callback);
        };
        wait_async(call, callback);
    }

    /**
     * Waiting for callback without result (void)
     *
     * \param callback - Callback that is invoked in thread owned by this event_loop
     * only before this function return
     * \param duration - Timeout (std::chrono::duration type)
     *
     * \return bool - if callback was invoked before duration expiration
     *
     */
    template <typename AsynchFunc, typename DurationType>
    bool wait(AsynchFunc&& callback, DurationType&& duration)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

        auto call = [this](auto callback) {
            this->post(callback);
        };
        return wait_async(call, callback, ms.count());
    }

    /**
     * \brief Waiting for finish for starting procedure
     * or immediately (if event loop has already started)
     * return
     *
     */
    void wait();

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
    std::unique_ptr<boost::asio::io_service::strand> _pstrand;
    boost::optional<boost::asio::io_service::work> _loop_maintainer;
    std::string _thread_name = "io_service loop";
    std::atomic_uint64_t _queue_size;
    std::unique_ptr<std::thread> _thread;
};

} // namespace server_lib
