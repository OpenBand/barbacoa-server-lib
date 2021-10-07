#include <server_lib/application.h>

#include <server_lib/logging_helper.h>
#include "logging_trace.h"
#include "logger_set_internal_group.h"

namespace server_lib {

application::main_loop::main_loop(const std::string& name)
    : event_loop(false)
{
    SRV_ASSERT(is_main(), "Invalid MAIN loop creation");

    if (!name.empty())
        change_thread_name(name);
}

void application::main_loop::start(std::function<void(void)> start_notify,
                                   std::function<void(void)> stop_notify,
                                   bool waiting_for_start)
{
    SRV_ASSERT(is_main_thread(), "Only for MAIN thread allowed");

    if (!is_running())
    {
        SRV_LOGC_TRACE("MAIN loop is starting");
    }

    event_loop::start(start_notify, stop_notify, waiting_for_start);
}

void application::main_loop::stop()
{
    if (is_running())
    {
        SRV_LOGC_TRACE("MAIN loop is stopping");
    }

    if (is_main_thread())
        event_loop::stop();
    else
    {
        post([this]() {
            stop();
        });
    }
}

} // namespace server_lib
