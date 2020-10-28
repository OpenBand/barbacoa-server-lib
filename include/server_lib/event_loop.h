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

/* To hold transport events (etc. from epool)
 * wrapped by boost::asio and a little bit more
 * that supposed by message queue
*/
class event_loop
{
public:
    using timer = server_lib::timer<event_loop>;
    using periodical_timer = server_lib::periodical_timer<event_loop>;

    event_loop(bool in_separate_thread = true);
    virtual ~event_loop();

    static bool is_main_thread();

    template <typename Handler>
    void post(Handler&& handler)
    {
        SRV_ASSERT(_pservice);
        _pservice->post(_strand.wrap(std::move(handler)));
    }

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

    virtual void start(std::function<void(void)> start_notify = nullptr, std::function<void(void)> stop_notify = nullptr);
    virtual void stop();
    bool is_running() const
    {
        return _is_running;
    }
    bool is_main() const
    {
        return _is_main;
    }

    template <typename Result, typename TAsynchFunc>
    Result wait_async(const Result initial_result, TAsynchFunc&& asynch_func)
    {
        auto call = [this](auto asynch_func) {
            this->post(asynch_func);
        };
        return wait_async_call(initial_result, call, asynch_func);
    }

protected:
    void run();

protected:
    bool _run_in_separate_thread = false;
    std::atomic_bool _is_running;
    std::atomic_bool _is_main;
    long _tid = 0;
    std::shared_ptr<boost::asio::io_service> _pservice;
    boost::asio::io_service::strand _strand;
    boost::optional<boost::asio::io_service::work> _loop_maintainer;
    std::unique_ptr<std::thread> _thread;
    std::string _thread_name = "io_service loop";

protected:
    friend class server_lib::timer<event_loop>;

    template <typename DurationType, typename Handler>
    void start_timer(DurationType&& duration, Handler&& callback)
    {
        SRV_ASSERT(_pservice);

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        auto timer = std::make_shared<boost::asio::deadline_timer>(*_pservice);

        timer->expires_from_now(boost::posix_time::milliseconds(ms.count()));
        timer->async_wait(_strand.wrap([timer, callback](const boost::system::error_code& ec) {
            if (!ec)
            {
                callback();
            }
        }));
    }

private:
    bool is_main_loop();
    void apply_thread_name();
};

class main_loop : public event_loop
{
public:
    main_loop(const std::string& name = {});

    void set_exit_callback(std::function<void(void)>);
    void exit(const int = 0);

    void start(std::function<void(void)> start_notify = nullptr, std::function<void(void)> stop_notify = nullptr) override;
    void stop() override;

private:
    std::function<void(void)> _exit_callback = nullptr;
};

} // namespace server_lib
