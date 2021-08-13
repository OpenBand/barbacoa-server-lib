#pragma once

#include <chrono>
#include <memory>

#include <server_lib/types.h>

namespace server_lib {

/**
 * \ingroup common
 *
 * Thread safe single shot timer
 */
template <typename EventLoop>
class timer
{
    DECLARE_PTR(id)
    class id
    {
    public:
        int value = 0;
    };

public:
    timer(EventLoop& el)
        : _el(el)
        , _current_timer_id(std::make_shared<id>())
    {
    }

    ~timer()
    {
        stop();
    }

    template <typename DurationType, typename Callback>
    void start(DurationType&& duration, Callback&& callback)
    {
        int timer_id = ++_current_timer_id->value;
        auto id = _current_timer_id;

        _el.start_timer(std::forward<DurationType>(duration), [=]() {
            if (id->value == timer_id)
                callback();
        });
    }


    virtual void stop()
    {
        ++_current_timer_id->value;
    }

private:
    EventLoop& _el;
    id_ptr _current_timer_id;
};

/**
 * \ingroup common
 *
 * Thread safe periodical timer
 */
template <typename EventLoop>
class periodical_timer
{
public:
    periodical_timer(EventLoop& el)
        : _timer(el)
    {
    }

    ~periodical_timer()
    {
        stop();
    }

    template <typename DurationType, typename Callback>
    void start(DurationType&& duration, Callback&& callback)
    {
        _timer.start(std::forward<DurationType>(duration), [=]() {
            this->start(duration, callback);
            callback();
        });
    }

    void stop()
    {
        _timer.stop();
    }

private:
    timer<EventLoop> _timer;
};


} // namespace server_lib
