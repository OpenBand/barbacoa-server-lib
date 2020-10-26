#pragma once

#include <chrono>
#include <memory>

#include <server_lib/types.h>

namespace server_lib {

template <typename Transport>
class timer
{
    DECLARE_PTR(id)
    class id
    {
    public:
        int value = 0;
    };

public:
    timer(Transport& transport)
        : _transport_layer(transport)
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
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        int timer_id = ++_current_timer_id->value;
        auto id = _current_timer_id;

        _transport_layer.start_timer(ms, [=]() {
            if (id->value == timer_id)
                callback();
        });
    }


    virtual void stop()
    {
        ++_current_timer_id->value;
    }

private:
    Transport& _transport_layer;
    id_ptr _current_timer_id;
};


template <typename Transport>
class periodical_timer
{
public:
    periodical_timer(Transport& transport)
        : _timer(transport)
    {
    }

    template <typename DurationType, typename Callback>
    void start(DurationType&& duration, Callback&& callback)
    {
        _timer.start(duration, [=]() {
            this->start(duration, callback);
            callback();
        });
    }

    void stop()
    {
        _timer.stop();
    }

private:
    timer<Transport> _timer;
};


} // namespace server_lib
