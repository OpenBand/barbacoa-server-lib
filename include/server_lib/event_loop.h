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

    void change_thread_name(const std::string&);

    /**
     * Start loop
     *
     * \param start_notify - Callback that is invoked in thread owned by
     * this event_loop when thread has started
     * \param stop_notify - Callback that is invoked in thread owned by
     * this event_loop when thread has stopped
     *
     */
    virtual void start(std::function<void(void)> start_notify = nullptr,
                       std::function<void(void)> stop_notify = nullptr);
    virtual void stop();

    bool is_running() const
    {
        return _is_running;
    }

    static bool is_main_thread();

    auto queue_size() const
    {
        return _queue_size.load();
    }

    bool is_main() const
    {
        return _is_main;
    }

    bool is_this_loop() const
    {
        return _id.load() == std::this_thread::get_id();
    }

    /**
     * Callback that is invoked in thread owned by this event_loop
     * Multiple invokes are executed in queue
     *
     * \param handler
     *
     */
    template <typename Handler>
    void post(Handler&& handler)
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
    }

    /**
     * Waiting for callback with result endlessly
     *
     * \param initial_result - Result for case when callback can't be invoked
     * \param asynch_func - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename Result, typename AsynchFunc>
    Result wait_async(Result&& initial_result, AsynchFunc&& asynch_func)
    {
        auto call = [this](auto asynch_func) {
            this->post(asynch_func);
        };
        return wait_async_call(std::forward<Result>(initial_result), call, asynch_func);
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
    Result wait_async(Result&& initial_result, AsynchFunc&& asynch_func, DurationType&& duration)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

        auto call = [this](auto asynch_func) {
            this->post(asynch_func);
        };
        return wait_async_call(std::forward<Result>(initial_result), call, asynch_func, ms.count());
    }

    /**
     * Callback that is invoked after certain timeout
     *
     * \param duration - Timeout (std::chrono::duration type)
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename DurationType, typename Handler>
    void start_timer(DurationType&& duration, Handler&& callback)
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
    }

protected:
    void run();

protected:
    const bool _run_in_separate_thread = false;
    std::atomic_bool _is_running;
    std::atomic_bool _is_main;
    long _tid = 0;
    std::shared_ptr<boost::asio::io_service> _pservice;
    boost::asio::io_service::strand _strand;
    boost::optional<boost::asio::io_service::work> _loop_maintainer;
    std::unique_ptr<std::thread> _thread;
    std::string _thread_name = "io_service loop";
    std::atomic_uint64_t _queue_size;
    std::atomic<std::thread::id> _id;

private:
    bool is_main_loop();
    void apply_thread_name();
};

/**
 * \ingroup common
 *
 * \brief This class wrap main thread to provide event_loop features
 */
class main_loop : public event_loop
{
public:
    main_loop(const std::string& name = {});

    void set_exit_callback(std::function<void(void)>);
    void exit(const int = 0);

    void start(std::function<void(void)> start_notify = nullptr,
               std::function<void(void)> stop_notify = nullptr) override;
    void stop() override;

private:
    std::function<void(void)> _exit_callback = nullptr;
};

} // namespace server_lib
