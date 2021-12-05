#include <server_lib/asserts.h>
#include <server_lib/application.h>

#include "logger_set_internal_group.h"

namespace server_lib {

application_main_loop::main_loop(const std::string& name)
    : event_loop(false)
{
    SRV_ASSERT(is_main_thread(), "Invalid MAIN loop creation");

    if (!name.empty())
        change_thread_name(name);
}

event_loop& application_main_loop::start()
{
    SRV_ASSERT(is_main_thread(), "Only for MAIN thread allowed");

    if (!is_running())
    {
        SRV_LOGC_TRACE("MAIN loop is starting");
    }

    return event_loop::start();
}

void application_main_loop::stop()
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
