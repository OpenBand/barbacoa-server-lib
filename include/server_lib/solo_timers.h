#pragma once

#include <server_lib/event_loop.h>

#include <string>

namespace server_lib {

template <template <typename> typename Timer>
class solo_timer_t : public Timer<event_loop>
{
    using base_timer = Timer<event_loop>;

public:
    solo_timer_t(const std::string& dedicated_th_name = "timer")
        : base_timer(_dedicated_el)
    {
        _dedicated_el.change_thread_name(dedicated_th_name);
    }

    template <typename DurationType, typename Callback>
    void start(DurationType&& duration, Callback&& callback)
    {
        _dedicated_el.on_start([this, duration = std::move(duration), callback = std::move(callback)]() {
                         this->base_timer::start(duration, callback);
                     })
            .start();
    }

    void stop() override
    {
        _dedicated_el.stop();
    }

private:
    event_loop _dedicated_el;
};

using solo_timer = solo_timer_t<timer>;
using solo_periodical_timer = solo_timer_t<periodical_timer>;

} // namespace server_lib
